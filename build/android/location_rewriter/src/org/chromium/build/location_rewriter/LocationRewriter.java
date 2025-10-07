// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.build.location_rewriter;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.stream.Collectors;
import java.util.stream.Stream;

/**
 * Processes all the class files in a directory and replaces invocations of methods listed in {@link
 * org.chromium.build.location_rewriter.LocationRewriterTargetMethods} with the corresponding
 * overload that takes in an additional Location object and also adds code to generate the Location
 * object populated with the source file name, function name and line number of the callsite.
 */
public class LocationRewriter {

    public LocationRewriter() {}

    private void maybeRewriteClassFile(Path classFile) throws IOException {
        LocationRewriterImpl impl = new LocationRewriterImpl();

        byte[] originalFileContents = Files.readAllBytes(classFile);
        byte[] transformedFileContents = impl.rewrite(originalFileContents);
        if (transformedFileContents != originalFileContents) {
            Files.write(classFile, transformedFileContents);
        }
    }

    private void processClassesDir(Path classesDir)
            throws IOException, InterruptedException, ExecutionException {
        // Create a fixed-size thread pool with a number of threads equal to the number of available
        // CPU cores.
        final int numThreads = Runtime.getRuntime().availableProcessors();
        ExecutorService executor = Executors.newFixedThreadPool(numThreads);
        List<Future<Void>> futures = new ArrayList<>();

        try (Stream<Path> paths = Files.walk(classesDir)) {
            List<Path> classFiles =
                    paths.filter(p -> Files.isRegularFile(p) && p.toString().endsWith(".class"))
                            .collect(Collectors.toList());

            for (Path filePath : classFiles) {
                Callable<Void> task =
                        () -> {
                            maybeRewriteClassFile(filePath);
                            return null; // Callable requires a return value.
                        };
                futures.add(executor.submit(task));
            }

            for (Future<Void> future : futures) {
                future.get();
            }
        } finally {
            executor.shutdown();
        }
    }

    public static void main(String[] args) {
        if (args.length != 1) {
            System.err.println("Usage: bin/helper/location_rewriter <classes_dir>");
            System.exit(1);
        }

        try {
            LocationRewriter rewriter = new LocationRewriter();
            rewriter.processClassesDir(Paths.get(args[0]));
        } catch (Exception e) {
            System.err.println("Failed with exception: " + e);
            System.exit(-1);
        }
    }
}
