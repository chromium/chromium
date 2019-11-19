// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import org.junit.Assert;

import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedInfo;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Arrays;

/**
 * Utilities for dealing with variations seeds.
 */
public class VariationsTestUtils {
    public static void assertSeedsEqual(SeedInfo expected, SeedInfo actual) {
        Assert.assertTrue("Expected " + expected + " but got " + actual,
                seedsEqual(expected, actual));
    }

    public static boolean seedsEqual(SeedInfo a, SeedInfo b) {
        return strEqual(a.signature, b.signature) && strEqual(a.country, b.country)
                && (a.date == b.date) && (a.isGzipCompressed == b.isGzipCompressed)
                && Arrays.equals(a.seedData, b.seedData);
    }

    private static boolean strEqual(String a, String b) {
        return a == null ? b == null : a.equals(b);
    }

    public static SeedInfo createMockSeed() {
        SeedInfo seed = new SeedInfo();
        seed.seedData = "bogus seed data".getBytes();
        seed.signature = "bogus seed signature";
        seed.country = "GB";
        seed.date = 946684800000L; // New Year's 2000 GMT
        return seed;
    }

    public static void writeMockSeed(File dest) throws IOException {
        FileOutputStream stream = null;
        try {
            stream = new FileOutputStream(dest);
            VariationsUtils.writeSeed(stream, createMockSeed());
        } finally {
            if (stream != null) stream.close();
        }
    }

    public static void deleteSeeds() throws IOException {
        deleteIfExists(VariationsUtils.getSeedFile());
        deleteIfExists(VariationsUtils.getNewSeedFile());
        deleteIfExists(VariationsUtils.getStampFile());
    }

    private static void deleteIfExists(File file) throws IOException {
        if (file.exists() && !file.delete()) {
            throw new IOException("Failed to delete " + file);
        }
    }
}
