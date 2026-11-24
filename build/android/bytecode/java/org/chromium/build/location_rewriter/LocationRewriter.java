// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.location_rewriter;

import org.chromium.bytecode.ClassHierarchy;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import java.util.zip.ZipOutputStream;

/**
 * Processes all the class files in a directory and replaces invocations of methods listed in {@link
 * org.chromium.build.location_rewriter.LocationRewriterTargetMethods} with the corresponding
 * overload that takes in an additional Location object and also adds code to generate the Location
 * object populated with the source file name, function name and line number of the callsite.
 */
public class LocationRewriter {
    LocationRewriterImpl mRewriterImpl;

    public LocationRewriter(ClassHierarchy classHierarchy) {
        mRewriterImpl = new LocationRewriterImpl(classHierarchy);
    }

    private void rewriteJarFile(File inputJar, File outputJar) throws IOException {
        try (InputStream inputStream = new BufferedInputStream(new FileInputStream(inputJar));
                OutputStream outputStream =
                        new BufferedOutputStream(new FileOutputStream(outputJar))) {
            rewriteZipFile(inputStream, outputStream);
        }
    }

    private void rewriteZipFile(InputStream inputStream, OutputStream outputStream)
            throws IOException {
        try (ZipOutputStream zipOutputStream = new ZipOutputStream(outputStream);
                ZipInputStream zipInputStream = new ZipInputStream(inputStream)) {
            ZipEntry entry;
            while ((entry = zipInputStream.getNextEntry()) != null) {
                rewriteZipEntry(zipInputStream, zipOutputStream, entry);
            }
            zipOutputStream.finish();
        }
    }

    private void rewriteZipEntry(
            ZipInputStream zipInputStream, ZipOutputStream zipOutputStream, ZipEntry entry)
            throws IOException {
        ZipEntry newEntry = new ZipEntry(entry.getName());
        newEntry.setTime(entry.getTime());
        zipOutputStream.putNextEntry(newEntry);

        byte[] fileContents = zipInputStream.readAllBytes();
        if (entry.getName().endsWith(".class")) {
            fileContents = rewriteClassFile(fileContents);
        }
        zipOutputStream.write(fileContents, 0, fileContents.length);
        zipOutputStream.closeEntry();
    }

    private byte[] rewriteClassFile(byte[] inputClassFileContents) {
        return mRewriterImpl.rewrite(inputClassFileContents);
    }

    public static void main(String[] args) {
        try {
            ClassHierarchy.ParsedArgs parsed = ClassHierarchy.ParsedArgs.parse(args);
            String[] positionalArgs = parsed.positionalArgs;
            if (positionalArgs.length != 2) {
                throw new IllegalArgumentException(
                        "Usage: location_rewriter [--classpath <cp>] <input jar> <output jar>");
            }

            File inputJar = new File(positionalArgs[0]);
            File outputJar = new File(positionalArgs[1]);
            if (!inputJar.exists()) {
                throw new FileNotFoundException("Input jar not found: " + inputJar.getPath());
            }

            List<String> classPathJars = new ArrayList<>(parsed.classpath);
            classPathJars.add(positionalArgs[0]);
            try (ClassHierarchy classHierarchy = new ClassHierarchy(classPathJars)) {
                LocationRewriter rewriter = new LocationRewriter(classHierarchy);
                rewriter.rewriteJarFile(inputJar, outputJar);
            }
        } catch (Exception e) {
            System.err.println("Failed to rewrite location (" + e + ")");
            System.exit(1);
        }
    }
}
