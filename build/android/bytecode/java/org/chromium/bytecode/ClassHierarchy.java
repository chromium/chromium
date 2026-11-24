// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Opcodes;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.net.URI; // nocheck
import java.nio.charset.StandardCharsets;
import java.nio.file.DirectoryStream;
import java.nio.file.FileSystem;
import java.nio.file.FileSystems;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.ServiceConfigurationError;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/**
 * Resolves class hierarchies and bytecode directly from classpath jars and jrt:/ without dynamic
 * class loading, allowing bytecode rewriters to run in GraalVM native-image binaries as well as on
 * the JVM.
 */
public class ClassHierarchy implements AutoCloseable {
    private static final String OBJECT_INTERNAL_NAME = "java/lang/Object";
    private static final String CLASS_SUFFIX = ".class";
    // Cap open ZipFiles to avoid exhausting file descriptors when processing hundreds of jars.
    private static final int MAX_OPEN_ZIPS = 128;
    // Switch from linear jar scanning to a full class-name index after this many lookups (when
    // there are enough jars to justify scanning central directories). Avoids enumerating zip
    // entries for short runs like location_rewriter while keeping trace_event_adder O(1).
    private static final int INDEX_THRESHOLD = 16;
    private static final int MIN_JARS_TO_INDEX = 8;
    private static final ClassInfo NOT_FOUND = new ClassInfo(/* isInterface= */ false, null, null);
    private static final ClassInfo OBJECT_INFO =
            new ClassInfo(/* isInterface= */ false, null, null);

    private final List<File> mJarFiles = new ArrayList<>();
    private final Map<Integer, ZipFile> mOpenZips =
            new LinkedHashMap<>(
                    /* initialCapacity= */ 32, /* loadFactor= */ 0.75f, /* accessOrder= */ true) {
                @Override
                protected boolean removeEldestEntry(Map.Entry<Integer, ZipFile> eldest) {
                    if (size() > MAX_OPEN_ZIPS) {
                        closeQuietly(eldest.getValue());
                        return true;
                    }
                    return false;
                }
            };
    private final Map<String, ClassInfo> mClassInfoCache = new HashMap<>();

    private Map<String, Integer> mEntryToIndex;
    private int mLookupCount;
    // Null until initialized; empty if jrt:/ is unavailable; otherwise [java.base, ...others].
    private List<Path> mJrtModules;

    /** Parsed CLI arguments separating --classpath entries from positional arguments. */
    public static final class ParsedArgs {
        public final List<String> classpath;
        public final String[] positionalArgs;

        private ParsedArgs(List<String> classpath, String[] positionalArgs) {
            this.classpath = classpath;
            this.positionalArgs = positionalArgs;
        }

        public static ParsedArgs parse(String[] args) throws IOException {
            List<String> expanded = new ArrayList<>(args.length);
            for (String arg : args) {
                if (arg.startsWith("@")) {
                    Path path = Paths.get(arg.substring(1));
                    expanded.addAll(Files.readAllLines(path, StandardCharsets.UTF_8));
                } else {
                    expanded.add(arg);
                }
            }

            List<String> classpath = new ArrayList<>();
            List<String> positional = new ArrayList<>(expanded.size());
            for (int i = 0; i < expanded.size(); i++) {
                String arg = expanded.get(i);
                if ("--classpath".equals(arg)) {
                    if (i + 1 >= expanded.size()) {
                        throw new IllegalArgumentException("Missing value for --classpath");
                    }
                    addSplitPaths(expanded.get(++i), classpath);
                } else if (arg.startsWith("--classpath=")) {
                    addSplitPaths(arg.substring("--classpath=".length()), classpath);
                } else {
                    positional.add(arg);
                }
            }
            return new ParsedArgs(classpath, positional.toArray(new String[0]));
        }

        private static void addSplitPaths(String colonSeparated, List<String> out) {
            for (String entry : colonSeparated.split(File.pathSeparator)) {
                if (!entry.isEmpty()) {
                    out.add(entry);
                }
            }
        }
    }

    /** Minimal type hierarchy metadata extracted from a class file header. */
    public static final class ClassInfo {
        public final boolean isInterface;
        public final String superName;
        public final String[] interfaces;

        ClassInfo(boolean isInterface, String superName, String[] interfaces) {
            this.isInterface = isInterface;
            this.superName = superName;
            this.interfaces = interfaces != null ? interfaces : new String[0];
        }
    }

    public ClassHierarchy(Collection<String> classpathEntries) {
        mClassInfoCache.put(OBJECT_INTERNAL_NAME, OBJECT_INFO);
        Set<String> seen = new LinkedHashSet<>();
        for (String path : classpathEntries) {
            if (path == null || path.isEmpty() || !seen.add(path)) {
                continue;
            }
            File file = new File(path);
            if (file.isFile()) {
                mJarFiles.add(file);
            }
        }
    }

    /** Records class header metadata from an already-parsed ClassReader. */
    public void registerClass(ClassReader reader) {
        String name = reader.getClassName();
        if (name == null) {
            return;
        }
        ClassInfo existing = mClassInfoCache.get(name);
        if (existing == null || existing == NOT_FOUND) {
            mClassInfoCache.put(name, classInfoFromReader(reader));
        }
    }

    /** Returns raw bytecode for the given internal class name, or null if not found. */
    public byte[] getClassBytes(String internalName) {
        String entryName = internalName + CLASS_SUFFIX;
        // Index before a second linear scan when jars exceed MAX_OPEN_ZIPS to avoid LRU thrashing.
        if (mEntryToIndex == null
                && mJarFiles.size() > MIN_JARS_TO_INDEX
                && (mLookupCount >= INDEX_THRESHOLD
                        || (mLookupCount >= 1 && mJarFiles.size() > MAX_OPEN_ZIPS))) {
            buildIndex();
        }
        mLookupCount++;

        if (mEntryToIndex == null) {
            for (int i = 0; i < mJarFiles.size(); i++) {
                byte[] bytes = readFromJar(i, entryName);
                if (bytes != null) {
                    return bytes;
                }
            }
        } else {
            Integer idx = mEntryToIndex.get(entryName);
            if (idx != null) {
                byte[] bytes = readFromJar(idx, entryName);
                if (bytes != null) {
                    return bytes;
                }
            }
        }
        return readFromJrt(entryName);
    }

    /** Returns ClassInfo for the given internal class name, or null if not found. */
    public ClassInfo getClassInfo(String internalName) {
        ClassInfo cached = mClassInfoCache.get(internalName);
        if (cached != null) {
            return cached == NOT_FOUND ? null : cached;
        }
        byte[] bytes = getClassBytes(internalName);
        if (bytes == null) {
            mClassInfoCache.put(internalName, NOT_FOUND);
            return null;
        }
        ClassInfo info = classInfoFromReader(new ClassReader(bytes));
        mClassInfoCache.put(internalName, info);
        return info;
    }

    /** Returns true if {@code subType} is assignable to {@code superType}. */
    public boolean isAssignableFrom(String superType, String subType) {
        return isAssignableFrom(superType, subType, /* missingTypeOut= */ null);
    }

    private static void recordMissing(String[] missingTypeOut, String type) {
        if (missingTypeOut != null && missingTypeOut[0] == null) {
            missingTypeOut[0] = type;
        }
    }

    private boolean isAssignableFrom(String superType, String subType, String[] missingTypeOut) {
        if (superType.equals(subType) || OBJECT_INTERNAL_NAME.equals(superType)) {
            return true;
        }
        ClassInfo superInfo = getClassInfo(superType);
        if (superInfo == null) {
            recordMissing(missingTypeOut, superType);
            return false;
        }
        ClassInfo subInfo = getClassInfo(subType);
        if (subInfo == null) {
            recordMissing(missingTypeOut, subType);
            return false;
        }
        if (!superInfo.isInterface) {
            if (subInfo.isInterface) {
                return false;
            }
            String cur = subInfo.superName;
            while (cur != null) {
                if (cur.equals(superType)) {
                    return true;
                }
                ClassInfo curInfo = getClassInfo(cur);
                if (curInfo == null) {
                    recordMissing(missingTypeOut, cur);
                    return false;
                }
                cur = curInfo.superName;
            }
            return false;
        }

        // superType is an interface: traverse superclasses and superinterfaces.
        if (subInfo.interfaces.length == 0
                && (subInfo.superName == null || OBJECT_INTERNAL_NAME.equals(subInfo.superName))) {
            return false;
        }
        ArrayDeque<ClassInfo> queue = new ArrayDeque<>();
        Set<String> visited = new HashSet<>();
        queue.add(subInfo);
        visited.add(subType);
        while (!queue.isEmpty()) {
            ClassInfo current = queue.poll();
            for (String iface : current.interfaces) {
                if (iface.equals(superType)) {
                    return true;
                }
                if (visited.add(iface)) {
                    ClassInfo ifaceInfo = getClassInfo(iface);
                    if (ifaceInfo != null) {
                        queue.add(ifaceInfo);
                    } else {
                        recordMissing(missingTypeOut, iface);
                    }
                }
            }
            String superName = current.superName;
            if (superName != null && visited.add(superName)) {
                ClassInfo parentInfo = getClassInfo(superName);
                if (parentInfo != null) {
                    queue.add(parentInfo);
                } else {
                    recordMissing(missingTypeOut, superName);
                }
            }
        }
        return false;
    }

    /** Computes the common superclass of two internal type names for ASM frame computation. */
    public String getCommonSuperClass(String type1, String type2) {
        if (type1.equals(type2)) {
            return type1;
        }
        if (OBJECT_INTERNAL_NAME.equals(type1) || OBJECT_INTERNAL_NAME.equals(type2)) {
            return OBJECT_INTERNAL_NAME;
        }
        if (type1.startsWith("[") || type2.startsWith("[")) {
            return OBJECT_INTERNAL_NAME;
        }
        ClassInfo info1 = getClassInfo(type1);
        if (info1 == null) {
            throw new TypeNotPresentException(type1, new ClassNotFoundException(type1));
        }
        ClassInfo info2 = getClassInfo(type2);
        if (info2 == null) {
            throw new TypeNotPresentException(type2, new ClassNotFoundException(type2));
        }
        if (info1.isInterface || info2.isInterface) {
            String[] missingType = new String[1];
            if (info1.isInterface && isAssignableFrom(type1, type2, missingType)) {
                return type1;
            }
            if (info2.isInterface && isAssignableFrom(type2, type1, missingType)) {
                return type2;
            }
            if (missingType[0] != null) {
                throw new TypeNotPresentException(
                        missingType[0], new ClassNotFoundException(missingType[0]));
            }
            return OBJECT_INTERNAL_NAME;
        }

        String missingType = null;
        Set<String> ancestors2 = new HashSet<>();
        String cur = type2;
        while (cur != null) {
            ancestors2.add(cur);
            ClassInfo curInfo = getClassInfo(cur);
            if (curInfo == null) {
                missingType = cur;
                break;
            }
            cur = curInfo.superName;
        }
        cur = type1;
        while (cur != null) {
            if (ancestors2.contains(cur)) {
                return cur;
            }
            ClassInfo curInfo = getClassInfo(cur);
            if (curInfo == null) {
                if (missingType == null) {
                    missingType = cur;
                }
                break;
            }
            cur = curInfo.superName;
        }
        if (missingType != null) {
            throw new TypeNotPresentException(missingType, new ClassNotFoundException(missingType));
        }
        return OBJECT_INTERNAL_NAME;
    }

    /** Creates an ASM ClassWriter backed by this ClassHierarchy for frame computation. */
    public ClassWriter createClassWriter(ClassReader reader, int flags) {
        return new ClassWriter(reader, flags) {
            @Override
            protected String getCommonSuperClass(String type1, String type2) {
                return ClassHierarchy.this.getCommonSuperClass(type1, type2);
            }
        };
    }

    @Override
    public void close() {
        for (ZipFile zipFile : mOpenZips.values()) {
            closeQuietly(zipFile);
        }
        mOpenZips.clear();
    }

    private static ClassInfo classInfoFromReader(ClassReader reader) {
        return new ClassInfo(
                (reader.getAccess() & Opcodes.ACC_INTERFACE) != 0,
                reader.getSuperName(),
                reader.getInterfaces());
    }

    private void buildIndex() {
        Map<String, Integer> index = new HashMap<>();
        for (int i = 0; i < mJarFiles.size(); i++) {
            ZipFile zf = mOpenZips.get(i);
            boolean openedHere = false;
            if (zf == null) {
                try {
                    zf = new ZipFile(mJarFiles.get(i));
                    openedHere = true;
                } catch (IOException e) {
                    continue;
                }
            }
            try {
                Enumeration<? extends ZipEntry> entries = zf.entries();
                while (entries.hasMoreElements()) {
                    String name = entries.nextElement().getName();
                    if (name.endsWith(CLASS_SUFFIX)) {
                        index.putIfAbsent(name, i);
                    }
                }
            } finally {
                if (openedHere) {
                    closeQuietly(zf);
                }
            }
        }
        mEntryToIndex = index;
    }

    private byte[] readFromJar(int index, String entryName) {
        ZipFile zf = getZipFile(index, mJarFiles.get(index));
        if (zf == null) {
            return null;
        }
        ZipEntry entry = zf.getEntry(entryName);
        if (entry == null) {
            return null;
        }
        try (InputStream is = zf.getInputStream(entry)) {
            return is.readAllBytes();
        } catch (IOException e) {
            return null;
        }
    }

    private ZipFile getZipFile(int index, File file) {
        ZipFile zf = mOpenZips.get(index);
        if (zf != null) {
            return zf;
        }
        try {
            zf = new ZipFile(file);
            mOpenZips.put(index, zf);
            return zf;
        } catch (IOException e) {
            return null;
        }
    }

    private byte[] readFromJrt(String entryName) {
        if (mJrtModules == null) {
            mJrtModules = initJrtModules();
        }
        for (Path moduleDir : mJrtModules) {
            Path candidate = moduleDir.resolve(entryName);
            if (Files.isRegularFile(candidate)) {
                try {
                    return Files.readAllBytes(candidate);
                } catch (IOException e) {
                    return null;
                }
            }
        }
        return null;
    }

    private static List<Path> initJrtModules() {
        String javaHome = System.getProperty("java.home");
        if (javaHome == null
                || (!new File(javaHome, "lib/modules").isFile()
                        && !new File(javaHome, "modules").isDirectory())) {
            return Collections.emptyList();
        }
        try {
            FileSystem jrtFs = FileSystems.getFileSystem(URI.create("jrt:/"));
            Path baseModule = jrtFs.getPath("/modules/java.base");
            List<Path> modules = new ArrayList<>();
            if (Files.isDirectory(baseModule)) {
                modules.add(baseModule);
            }
            try (DirectoryStream<Path> ds = Files.newDirectoryStream(jrtFs.getPath("/modules"))) {
                for (Path moduleDir : ds) {
                    Path fileName = moduleDir.getFileName();
                    if (fileName != null && !"java.base".equals(fileName.toString())) {
                        modules.add(moduleDir);
                    }
                }
            }
            return modules;
        } catch (Exception | InternalError | LinkageError | ServiceConfigurationError e) {
            return Collections.emptyList();
        }
    }

    private static void closeQuietly(ZipFile zf) {
        try {
            zf.close();
        } catch (IOException e) {
            // Ignore close errors.
        }
    }
}
