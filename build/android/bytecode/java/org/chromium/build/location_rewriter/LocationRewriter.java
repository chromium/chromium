// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.location_rewriter;

import org.chromium.bytecode.ByteCodeRewriter;
import org.chromium.bytecode.ClassHierarchy;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

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
        ByteCodeRewriter.rewriteJar(
                inputJar, outputJar, (name, bytes) -> mRewriterImpl.rewrite(bytes));
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
