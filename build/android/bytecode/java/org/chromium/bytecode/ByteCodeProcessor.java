// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.bytecode;

import org.objectweb.asm.ClassReader;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Java application that takes in an input jar, performs a series of bytecode
 * transformations, and generates an output jar.
 */
class ByteCodeProcessor {
    private static final String CLASS_FILE_SUFFIX = ".class";
    private static final int BUFFER_SIZE = 16384;
    private static boolean sVerbose;
    private static boolean sIsPrebuilt;
    private static ClassLoader sDirectClassPathClassLoader;
    private static ClassLoader sFullClassPathClassLoader;
    private static Set<String> sFullClassPathJarPaths;
    private static Set<String> sMissingClassesAllowlist;
    private static Map<String, String> sJarToGnTarget;
    private static ClassPathValidator sValidator;

    private static Void processEntry(ZipEntry entry, byte[] data) {
        ClassReader reader = new ClassReader(data);
        if (sIsPrebuilt) {
            sValidator.validateFullClassPath(
                    reader, sFullClassPathClassLoader, sMissingClassesAllowlist);
        } else {
            sValidator.validateDirectClassPath(reader, sDirectClassPathClassLoader,
                    sFullClassPathClassLoader, sFullClassPathJarPaths, sMissingClassesAllowlist,
                    sVerbose);
        }
        return null;
    }

    private static void process(String gnTarget, String inputJarPath)
            throws ExecutionException, InterruptedException {
        ExecutorService executorService =
                Executors.newFixedThreadPool(Runtime.getRuntime().availableProcessors());
        try (ZipInputStream inputStream = new ZipInputStream(
                     new BufferedInputStream(new FileInputStream(inputJarPath)))) {
            while (true) {
                ZipEntry entry = inputStream.getNextEntry();
                if (entry == null) {
                    break;
                }
                byte[] data = readAllBytes(inputStream);
                executorService.submit(() -> processEntry(entry, data));
            }
            executorService.shutdown(); // This is essential in order to avoid waiting infinitely.
            executorService.awaitTermination(1, TimeUnit.HOURS);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }

        if (sValidator.hasErrors()) {
            sValidator.printAll(gnTarget, sJarToGnTarget);
            System.exit(1);
        }
    }

    private static byte[] readAllBytes(InputStream inputStream) throws IOException {
        ByteArrayOutputStream buffer = new ByteArrayOutputStream();
        int numRead = 0;
        byte[] data = new byte[BUFFER_SIZE];
        while ((numRead = inputStream.read(data, 0, data.length)) != -1) {
            buffer.write(data, 0, numRead);
        }
        return buffer.toByteArray();
    }

    /**
     * Loads a list of jars and returns a ClassLoader capable of loading all classes found in the
     * given jars.
     */
    static ClassLoader loadJars(Collection<String> paths) {
        URL[] jarUrls = new URL[paths.size()];
        int i = 0;
        for (String path : paths) {
            try {
                jarUrls[i++] = new File(path).toURI().toURL();
            } catch (MalformedURLException e) {
                throw new RuntimeException(e);
            }
        }
        return new URLClassLoader(jarUrls);
    }

    /**
     * Extracts a length-encoded list of strings from the arguments, and adds them to |out|. Returns
     * the new "next index" to be processed.
     */
    private static int parseListArgument(String[] args, int index, Collection<String> out) {
        int argLength = Integer.parseInt(args[index++]);
        out.addAll(Arrays.asList(Arrays.copyOfRange(args, index, index + argLength)));
        return index + argLength;
    }

    public static void main(String[] args) throws ClassPathValidator.ClassNotLoadedException,
                                                  ExecutionException, InterruptedException {
        // Invoke this script using //build/android/gyp/bytecode_processor.py
        int currIndex = 0;
        String gnTarget = args[currIndex++];
        String inputJarPath = args[currIndex++];
        sVerbose = args[currIndex++].equals("--verbose");
        sIsPrebuilt = args[currIndex++].equals("--is-prebuilt");

        sMissingClassesAllowlist = new HashSet<>();
        currIndex = parseListArgument(args, currIndex, sMissingClassesAllowlist);

        ArrayList<String> sdkJarPaths = new ArrayList<>();
        currIndex = parseListArgument(args, currIndex, sdkJarPaths);

        ArrayList<String> directClassPathJarPaths = new ArrayList<>();
        directClassPathJarPaths.add(inputJarPath);
        directClassPathJarPaths.addAll(sdkJarPaths);
        currIndex = parseListArgument(args, currIndex, directClassPathJarPaths);
        sDirectClassPathClassLoader = loadJars(directClassPathJarPaths);

        ArrayList<String> fullClassPathJarPaths = new ArrayList<>();
        currIndex = parseListArgument(args, currIndex, fullClassPathJarPaths);
        ArrayList<String> gnTargets = new ArrayList<>();
        parseListArgument(args, currIndex, gnTargets);
        sJarToGnTarget = new HashMap<>();
        assert fullClassPathJarPaths.size() == gnTargets.size();
        for (int i = 0; i < fullClassPathJarPaths.size(); ++i) {
            sJarToGnTarget.put(fullClassPathJarPaths.get(i), gnTargets.get(i));
        }

        // Load all jars that are on the classpath for the input jar for analyzing class
        // hierarchy.
        sFullClassPathJarPaths = new HashSet<>();
        sFullClassPathJarPaths.add(inputJarPath);
        sFullClassPathJarPaths.addAll(sdkJarPaths);
        sFullClassPathJarPaths.addAll(fullClassPathJarPaths);
        sFullClassPathClassLoader = loadJars(sFullClassPathJarPaths);
        sFullClassPathJarPaths.removeAll(directClassPathJarPaths);

        sValidator = new ClassPathValidator();
        process(gnTarget, inputJarPath);
    }
}
