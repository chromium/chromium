// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.ClassWriter;

import org.chromium.build.StoredZipWriter;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.util.Enumeration;
import java.util.function.BiFunction;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

/** Base class for scripts that perform bytecode modifications on a jar file. */
public abstract class ByteCodeRewriter {
    private static final String CLASS_FILE_SUFFIX = ".class";
    private final ClassHierarchy mClassHierarchy;

    public ByteCodeRewriter(ClassHierarchy classHierarchy) {
        mClassHierarchy = classHierarchy;
    }

    public ClassHierarchy getClassHierarchy() {
        return mClassHierarchy;
    }

    public void rewrite(File inputJar, File outputJar) throws IOException {
        rewriteJar(inputJar, outputJar, this::processClassEntry);
    }

    /**
     * Rewrites {@code inputJar} into an uncompressed {@code outputJar}, passing each {@code .class}
     * entry to {@code classRewriter} and reusing existing CRC32 values for unmodified entries.
     */
    public static void rewriteJar(
            File inputJar, File outputJar, BiFunction<String, byte[], byte[]> classRewriter)
            throws IOException {
        if (!inputJar.exists()) {
            throw new FileNotFoundException("Input jar not found: " + inputJar.getPath());
        }

        try (ZipFile zipFile = new ZipFile(inputJar);
                StoredZipWriter writer = new StoredZipWriter(outputJar.toPath())) {
            Enumeration<? extends ZipEntry> entries = zipFile.entries();
            while (entries.hasMoreElements()) {
                ZipEntry entry = entries.nextElement();
                String entryName = entry.getName();
                byte[] inputBytes = readZipEntryBytes(zipFile, entry);
                byte[] outputBytes = inputBytes;
                if (entryName.endsWith(CLASS_FILE_SUFFIX)) {
                    outputBytes = classRewriter.apply(entryName, inputBytes);
                }
                if (outputBytes == inputBytes) {
                    writer.writeEntry(entryName, outputBytes, entry.getCrc());
                } else {
                    writer.writeEntry(entryName, outputBytes);
                }
            }
            writer.finish();
        }
    }

    /** Returns true if the class at the given path in the archive should be rewritten. */
    protected abstract boolean shouldRewriteClass(String classPath);

    /** Returns true if the class at the given {@link ClassReader} should be rewritten. */
    protected boolean shouldRewriteClass(ClassReader classReader) {
        return true;
    }

    /**
     * Returns the ClassVisitor that should be used to modify the bytecode of class at the given
     * path in the archive.
     */
    protected abstract ClassVisitor getClassVisitorForClass(
            String classPath, ClassVisitor delegate);

    private static byte[] readZipEntryBytes(ZipFile zipFile, ZipEntry entry) throws IOException {
        long size = entry.getSize();
        try (InputStream is = zipFile.getInputStream(entry)) {
            if (size >= 0 && size <= Integer.MAX_VALUE) {
                return is.readNBytes((int) size);
            }
            return is.readAllBytes();
        }
    }

    private byte[] processClassEntry(String entryName, byte[] entryBytes) {
        if (!shouldRewriteClass(entryName)) {
            return entryBytes;
        }
        try {
            ClassReader reader = new ClassReader(entryBytes);
            mClassHierarchy.registerClass(reader);
            if (!shouldRewriteClass(reader)) {
                return entryBytes;
            }
            ClassWriter writer =
                    mClassHierarchy.createClassWriter(reader, ClassWriter.COMPUTE_FRAMES);
            ClassVisitor classVisitor = getClassVisitorForClass(entryName, writer);
            reader.accept(classVisitor, ClassReader.EXPAND_FRAMES);

            writer.visitEnd();
            return writer.toByteArray();
        } catch (Throwable e) {
            throw new RuntimeException("Failed when processing " + entryName, e);
        }
    }
}
