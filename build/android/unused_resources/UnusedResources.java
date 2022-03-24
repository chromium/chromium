/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Modifications are owned by the Chromium Authors.
// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package build.android.unused_resources;

import static com.android.ide.common.symbols.SymbolIo.readFromAapt;
import static com.android.utils.SdkUtils.endsWithIgnoreCase;
import static com.google.common.base.Charsets.UTF_8;

import com.android.ide.common.resources.usage.ResourceUsageModel;
import com.android.ide.common.resources.usage.ResourceUsageModel.Resource;
import com.android.ide.common.symbols.Symbol;
import com.android.ide.common.symbols.SymbolTable;
import com.android.resources.ResourceFolderType;
import com.android.resources.ResourceType;
import com.android.tools.r8.CompilationFailedException;
import com.android.tools.r8.ProgramResource;
import com.android.tools.r8.ProgramResourceProvider;
import com.android.tools.r8.ResourceShrinker;
import com.android.tools.r8.ResourceShrinker.Command;
import com.android.tools.r8.ResourceShrinker.ReferenceChecker;
import com.android.tools.r8.origin.PathOrigin;
import com.android.utils.XmlUtils;
import com.google.common.base.Charsets;
import com.google.common.base.Joiner;
import com.google.common.collect.Maps;
import com.google.common.io.ByteStreams;
import com.google.common.io.Closeables;
import com.google.common.io.Files;

import org.w3c.dom.Document;
import org.w3c.dom.Node;
import org.xml.sax.SAXException;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ExecutionException;
import java.util.stream.Collectors;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import javax.xml.parsers.ParserConfigurationException;

/**
  Copied with modifications from gradle core source
  https://android.googlesource.com/platform/tools/base/+/master/build-system/gradle-core/src/main/groovy/com/android/build/gradle/tasks/ResourceUsageAnalyzer.java

  Modifications are mostly to:
    - Remove unused code paths to reduce complexity.
    - Reduce dependencies unless absolutely required.
*/

public class UnusedResources {
    private static final String ANDROID_RES = "android_res/";
    private static final String DOT_DEX = ".dex";
    private static final String DOT_CLASS = ".class";
    private static final String DOT_XML = ".xml";
    private static final String DOT_JAR = ".jar";
    private static final String FN_RESOURCE_TEXT = "R.txt";

    /* A source of resource classes to track, can be either a folder or a jar */
    private final Iterable<File> mRTxtFiles;
    private final File mProguardMapping;
    /** These can be class or dex files. */
    private final Iterable<File> mClasses;
    private final Iterable<File> mManifests;
    private final Iterable<File> mResourceDirs;

    private final File mReportFile;
    private final StringWriter mDebugOutput;
    private final PrintWriter mDebugPrinter;

    /** The computed set of unused resources */
    private List<Resource> mUnused;

    /**
     * Map from resource class owners (VM format class) to corresponding resource entries.
     * This lets us map back from code references (obfuscated class and possibly obfuscated field
     * reference) back to the corresponding resource type and name.
     */
    private Map<String, Pair<ResourceType, Map<String, String>>> mResourceObfuscation =
            Maps.newHashMapWithExpectedSize(30);

    /** Obfuscated name of android/support/v7/widget/SuggestionsAdapter.java */
    private String mSuggestionsAdapter;

    /** Obfuscated name of android/support/v7/internal/widget/ResourcesWrapper.java */
    private String mResourcesWrapper;

    /* A Pair class because java does not come with batteries included. */
    private static class Pair<U, V> {
        private U mFirst;
        private V mSecond;

        Pair(U first, V second) {
            this.mFirst = first;
            this.mSecond = second;
        }

        public U getFirst() {
            return mFirst;
        }

        public V getSecond() {
            return mSecond;
        }
    }

    public UnusedResources(Iterable<File> rTxtFiles, Iterable<File> classes,
            Iterable<File> manifests, File mapping, Iterable<File> resources, File reportFile) {
        mRTxtFiles = rTxtFiles;
        mProguardMapping = mapping;
        mClasses = classes;
        mManifests = manifests;
        mResourceDirs = resources;

        mReportFile = reportFile;
        if (reportFile != null) {
            mDebugOutput = new StringWriter(8 * 1024);
            mDebugPrinter = new PrintWriter(mDebugOutput);
        } else {
            mDebugOutput = null;
            mDebugPrinter = null;
        }
    }

    public void close() {
        if (mDebugOutput != null) {
            String output = mDebugOutput.toString();

            if (mReportFile != null) {
                File dir = mReportFile.getParentFile();
                if (dir != null) {
                    if ((dir.exists() || dir.mkdir()) && dir.canWrite()) {
                        try {
                            Files.asCharSink(mReportFile, Charsets.UTF_8).write(output);
                        } catch (IOException ignore) {
                        }
                    }
                }
            }
        }
    }

    public void analyze() throws IOException, ParserConfigurationException, SAXException {
        gatherResourceValues(mRTxtFiles);
        recordMapping(mProguardMapping);

        for (File jarOrDir : mClasses) {
            recordClassUsages(jarOrDir);
        }
        recordManifestUsages(mManifests);
        recordResources(mResourceDirs);
        dumpReferences();
        mModel.processToolsAttributes();
        mUnused = mModel.findUnused();
    }

    public void emitConfig(Path destination) throws IOException {
        File destinationFile = destination.toFile();
        if (!destinationFile.exists()) {
            destinationFile.getParentFile().mkdirs();
            boolean success = destinationFile.createNewFile();
            if (!success) {
                throw new IOException("Could not create " + destination);
            }
        }
        StringBuilder sb = new StringBuilder();
        Collections.sort(mUnused);
        for (Resource resource : mUnused) {
            sb.append(resource.type + "/" + resource.name + "#remove\n");
        }
        Files.asCharSink(destinationFile, UTF_8).write(sb.toString());
    }

    private void dumpReferences() {
        if (mDebugPrinter != null) {
            mDebugPrinter.print(mModel.dumpReferences());
        }
    }

    private void recordResources(Iterable<File> resources)
            throws IOException, SAXException, ParserConfigurationException {
        for (File resDir : resources) {
            File[] resourceFolders = resDir.listFiles();
            assert resourceFolders != null : "Invalid resource directory " + resDir;
            for (File folder : resourceFolders) {
                ResourceFolderType folderType = ResourceFolderType.getFolderType(folder.getName());
                if (folderType != null) {
                    recordResources(folderType, folder);
                }
            }
        }
    }

    private void recordResources(ResourceFolderType folderType, File folder)
            throws ParserConfigurationException, SAXException, IOException {
        File[] files = folder.listFiles();
        if (files != null) {
            for (File file : files) {
                String path = file.getPath();
                mModel.file = file;
                try {
                    boolean isXml = endsWithIgnoreCase(path, DOT_XML);
                    if (isXml) {
                        String xml = Files.toString(file, UTF_8);
                        Document document = XmlUtils.parseDocument(xml, true);
                        mModel.visitXmlDocument(file, folderType, document);
                    } else {
                        mModel.visitBinaryResource(folderType, file);
                    }
                } finally {
                    mModel.file = null;
                }
            }
        }
    }

    void recordMapping(File mapping) throws IOException {
        if (mapping == null || !mapping.exists()) {
            return;
        }
        final String arrowString = " -> ";
        final String resourceString = ".R$";
        Map<String, String> nameMap = null;
        for (String line : Files.readLines(mapping, UTF_8)) {
            if (line.startsWith(" ") || line.startsWith("\t")) {
                if (nameMap != null) {
                    // We're processing the members of a resource class: record names into the map
                    int n = line.length();
                    int i = 0;
                    for (; i < n; i++) {
                        if (!Character.isWhitespace(line.charAt(i))) {
                            break;
                        }
                    }
                    if (i < n && line.startsWith("int", i)) { // int or int[]
                        int start = line.indexOf(' ', i + 3) + 1;
                        int arrow = line.indexOf(arrowString);
                        if (start > 0 && arrow != -1) {
                            int end = line.indexOf(' ', start + 1);
                            if (end != -1) {
                                String oldName = line.substring(start, end);
                                String newName =
                                        line.substring(arrow + arrowString.length()).trim();
                                if (!newName.equals(oldName)) {
                                    nameMap.put(newName, oldName);
                                }
                            }
                        }
                    }
                }
                continue;
            } else {
                nameMap = null;
            }
            int index = line.indexOf(resourceString);
            if (index == -1) {
                // Record obfuscated names of a few known appcompat usages of
                // Resources#getIdentifier that are unlikely to be used for general
                // resource name reflection
                if (line.startsWith("android.support.v7.widget.SuggestionsAdapter ")) {
                    mSuggestionsAdapter =
                            line.substring(line.indexOf(arrowString) + arrowString.length(),
                                        line.indexOf(':') != -1 ? line.indexOf(':') : line.length())
                                    .trim()
                                    .replace('.', '/')
                            + DOT_CLASS;
                } else if (line.startsWith("android.support.v7.internal.widget.ResourcesWrapper ")
                        || line.startsWith("android.support.v7.widget.ResourcesWrapper ")
                        || (mResourcesWrapper == null // Recently wrapper moved
                                && line.startsWith(
                                        "android.support.v7.widget.TintContextWrapper$TintResources "))) {
                    mResourcesWrapper =
                            line.substring(line.indexOf(arrowString) + arrowString.length(),
                                        line.indexOf(':') != -1 ? line.indexOf(':') : line.length())
                                    .trim()
                                    .replace('.', '/')
                            + DOT_CLASS;
                }
                continue;
            }
            int arrow = line.indexOf(arrowString, index + 3);
            if (arrow == -1) {
                continue;
            }
            String typeName = line.substring(index + resourceString.length(), arrow);
            ResourceType type = ResourceType.fromClassName(typeName);
            if (type == null) {
                continue;
            }
            int end = line.indexOf(':', arrow + arrowString.length());
            if (end == -1) {
                end = line.length();
            }
            String target = line.substring(arrow + arrowString.length(), end).trim();
            String ownerName = target.replace('.', '/');

            nameMap = Maps.newHashMap();
            Pair<ResourceType, Map<String, String>> pair = new Pair(type, nameMap);
            mResourceObfuscation.put(ownerName, pair);
            // For fast lookup in isResourceClass
            mResourceObfuscation.put(ownerName + DOT_CLASS, pair);
        }
    }

    private void recordManifestUsages(File manifest)
            throws IOException, ParserConfigurationException, SAXException {
        String xml = Files.toString(manifest, UTF_8);
        Document document = XmlUtils.parseDocument(xml, true);
        mModel.visitXmlDocument(manifest, null, document);
    }

    private void recordManifestUsages(Iterable<File> manifests)
            throws IOException, ParserConfigurationException, SAXException {
        for (File manifest : manifests) {
            recordManifestUsages(manifest);
        }
    }

    private void recordClassUsages(File file) throws IOException {
        assert file.isFile();
        if (file.getPath().endsWith(DOT_DEX)) {
            byte[] bytes = Files.toByteArray(file);
            recordClassUsages(file, file.getName(), bytes);
        } else if (file.getPath().endsWith(DOT_JAR)) {
            ZipInputStream zis = null;
            try {
                FileInputStream fis = new FileInputStream(file);
                try {
                    zis = new ZipInputStream(fis);
                    ZipEntry entry = zis.getNextEntry();
                    while (entry != null) {
                        String name = entry.getName();
                        if (name.endsWith(DOT_DEX)) {
                            byte[] bytes = ByteStreams.toByteArray(zis);
                            if (bytes != null) {
                                recordClassUsages(file, name, bytes);
                            }
                        }

                        entry = zis.getNextEntry();
                    }
                } finally {
                    Closeables.close(fis, true);
                }
            } finally {
                Closeables.close(zis, true);
            }
        }
    }

    private void recordClassUsages(File file, String name, byte[] bytes) {
        assert name.endsWith(DOT_DEX);
        ReferenceChecker callback = new ReferenceChecker() {
            @Override
            public boolean shouldProcess(String internalName) {
                return !isResourceClass(internalName + DOT_CLASS);
            }

            @Override
            public void referencedInt(int value) {
                UnusedResources.this.referencedInt("dex", value, file, name);
            }

            @Override
            public void referencedString(String value) {
                // do nothing.
            }

            @Override
            public void referencedStaticField(String internalName, String fieldName) {
                Resource resource = getResourceFromCode(internalName, fieldName);
                if (resource != null) {
                    ResourceUsageModel.markReachable(resource);
                }
            }

            @Override
            public void referencedMethod(
                    String internalName, String methodName, String methodDescriptor) {
                // Do nothing.
            }
        };
        ProgramResource resource = ProgramResource.fromBytes(
                new PathOrigin(file.toPath()), ProgramResource.Kind.DEX, bytes, null);
        ProgramResourceProvider provider = () -> Arrays.asList(resource);
        try {
            Command command =
                    (new ResourceShrinker.Builder()).addProgramResourceProvider(provider).build();
            ResourceShrinker.run(command, callback);
        } catch (CompilationFailedException e) {
            e.printStackTrace();
        } catch (IOException e) {
            e.printStackTrace();
        } catch (ExecutionException e) {
            e.printStackTrace();
        }
    }

    /** Returns whether the given class file name points to an aapt-generated compiled R class. */
    boolean isResourceClass(String name) {
        if (mResourceObfuscation.containsKey(name)) {
            return true;
        }
        int index = name.lastIndexOf('/');
        if (index != -1 && name.startsWith("R$", index + 1) && name.endsWith(DOT_CLASS)) {
            String typeName = name.substring(index + 3, name.length() - DOT_CLASS.length());
            return ResourceType.fromClassName(typeName) != null;
        }
        return false;
    }

    Resource getResourceFromCode(String owner, String name) {
        Pair<ResourceType, Map<String, String>> pair = mResourceObfuscation.get(owner);
        if (pair != null) {
            ResourceType type = pair.getFirst();
            Map<String, String> nameMap = pair.getSecond();
            String renamedField = nameMap.get(name);
            if (renamedField != null) {
                name = renamedField;
            }
            return mModel.getResource(type, name);
        }
        if (isValidResourceType(owner)) {
            ResourceType type =
                    ResourceType.fromClassName(owner.substring(owner.lastIndexOf('$') + 1));
            if (type != null) {
                return mModel.getResource(type, name);
            }
        }
        return null;
    }

    private Boolean isValidResourceType(String candidateString) {
        return candidateString.contains("/")
                && candidateString.substring(candidateString.lastIndexOf('/') + 1).contains("$");
    }

    private void gatherResourceValues(Iterable<File> rTxts) throws IOException {
        for (File rTxt : rTxts) {
            assert rTxt.isFile();
            assert rTxt.getName().endsWith(FN_RESOURCE_TEXT);
            addResourcesFromRTxtFile(rTxt);
        }
    }

    private void addResourcesFromRTxtFile(File file) {
        try {
            SymbolTable st = readFromAapt(file, null);
            for (Symbol symbol : st.getSymbols().values()) {
                String symbolValue = symbol.getValue();
                if (symbol.getResourceType() == ResourceType.STYLEABLE) {
                    if (symbolValue.trim().startsWith("{")) {
                        // Only add the styleable parent, styleable children are not yet supported.
                        mModel.addResource(symbol.getResourceType(), symbol.getName(), null);
                    }
                } else {
                    mModel.addResource(symbol.getResourceType(), symbol.getName(), symbolValue);
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    ResourceUsageModel getModel() {
        return mModel;
    }

    private void referencedInt(String context, int value, File file, String currentClass) {
        Resource resource = mModel.getResource(value);
        if (ResourceUsageModel.markReachable(resource) && mDebugPrinter != null) {
            mDebugPrinter.println("Marking " + resource + " reachable: referenced from " + context
                    + " in " + file + ":" + currentClass);
        }
    }

    private final ResourceShrinkerUsageModel mModel = new ResourceShrinkerUsageModel();

    private class ResourceShrinkerUsageModel extends ResourceUsageModel {
        public File file;

        /**
         * Whether we should ignore tools attribute resource references.
         * <p>
         * For example, for resource shrinking we want to ignore tools attributes,
         * whereas for resource refactoring on the source code we do not.
         *
         * @return whether tools attributes should be ignored
         */
        @Override
        protected boolean ignoreToolsAttributes() {
            return true;
        }

        @Override
        protected void onRootResourcesFound(List<Resource> roots) {
            if (mDebugPrinter != null) {
                mDebugPrinter.println(
                        "\nThe root reachable resources are:\n" + Joiner.on(",\n   ").join(roots));
            }
        }

        @Override
        protected Resource declareResource(ResourceType type, String name, Node node) {
            Resource resource = super.declareResource(type, name, node);
            resource.addLocation(file);
            return resource;
        }

        @Override
        protected void referencedString(String string) {
            // Do nothing
        }
    }

    public static void main(String[] args) throws Exception {
        List<File> rTxtFiles = null; // R.txt files
        List<File> classes = null; // Dex/jar w dex
        List<File> manifests = null; // manifests
        File mapping = null; // mapping
        List<File> resources = null; // resources dirs
        File log = null; // output log for debugging
        Path configPath = null; // output config
        for (int i = 0; i < args.length; i += 2) {
            switch (args[i]) {
                case "--rtxts":
                    rTxtFiles = Arrays.stream(args[i + 1].split(":"))
                                        .map(s -> new File(s))
                                        .collect(Collectors.toList());
                    break;
                case "--dexes":
                    classes = Arrays.stream(args[i + 1].split(":"))
                                      .map(s -> new File(s))
                                      .collect(Collectors.toList());
                    break;
                case "--manifests":
                    manifests = Arrays.stream(args[i + 1].split(":"))
                                        .map(s -> new File(s))
                                        .collect(Collectors.toList());
                    break;
                case "--mapping":
                    mapping = new File(args[i + 1]);
                    break;
                case "--resourceDirs":
                    resources = Arrays.stream(args[i + 1].split(":"))
                                        .map(s -> new File(s))
                                        .collect(Collectors.toList());
                    break;
                case "--log":
                    log = new File(args[i + 1]);
                    break;
                case "--outputConfig":
                    configPath = Paths.get(args[i + 1]);
                    break;
                default:
                    throw new IllegalArgumentException(args[i] + " is not a valid arg.");
            }
        }
        UnusedResources unusedResources =
                new UnusedResources(rTxtFiles, classes, manifests, mapping, resources, log);
        unusedResources.analyze();
        unusedResources.close();
        unusedResources.emitConfig(configPath);
    }
}
