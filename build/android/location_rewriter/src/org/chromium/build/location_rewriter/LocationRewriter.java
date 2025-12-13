// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.location_rewriter;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
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

    public LocationRewriter() {
        mRewriterImpl = new LocationRewriterImpl();
    }

    private void rewriteJarFile(File inputJar, File outputJar) throws IOException {
        try (InputStream inputStream = new BufferedInputStream(new FileInputStream(inputJar));
                OutputStream outputStream = new FileOutputStream(outputJar)) {
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
            if (args.length != 2) {
                throw new IllegalArgumentException(
                        "Usage: bin/helper/location_rewriter <input jar> <output jar>");
            }

            File inputJar = new File(args[0]);
            File outputJar = new File(args[1]);
            if (!inputJar.exists()) {
                throw new FileNotFoundException("Input jar not found: " + inputJar.getPath());
            }

            LocationRewriter rewriter = new LocationRewriter();
            rewriter.rewriteJarFile(inputJar, outputJar);
        } catch (Exception e) {
            System.err.println("Failed to rewrite location (" + e + ")");
            System.exit(1);
        }
    }
}
