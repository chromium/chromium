// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.common.variations;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import androidx.test.filters.MediumTest;

import com.google.protobuf.ByteString;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.android_webview.proto.AwVariationsSeedOuterClass.AwVariationsSeed;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.OnlyRunIn;
import org.chromium.android_webview.test.util.VariationsTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedInfo;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Arrays;

/** Test reading and writing variations seeds. */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS) // These are unit tests
@Batch(Batch.UNIT_TESTS)
public class VariationsUtilsTest {
    @Test
    @MediumTest
    public void testWriteAndReadSeed() throws IOException {
        File file = null;
        try {
            file = File.createTempFile("seed", null, null);
            VariationsTestUtils.writeMockSeed(file);
            SeedInfo readSeed = VariationsUtils.readSeedFile(file);
            VariationsTestUtils.assertSeedsEqual(VariationsTestUtils.createMockSeed(), readSeed);
        } finally {
            if (file != null) file.delete();
        }
    }

    // Test reading a seed that has some but not all fields, which should fail.
    @Test
    @MediumTest
    public void testReadSeedMissingFields() throws IOException {
        File file = null;
        try {
            file = File.createTempFile("seed", null, null);
            FileOutputStream stream = null;
            try {
                // Create a seed that's missing some fields.
                stream = new FileOutputStream(file);
                SeedInfo info = VariationsTestUtils.createMockSeed();
                AwVariationsSeed proto =
                        AwVariationsSeed.newBuilder()
                                .setSignature(info.signature)
                                .setCountry(info.country)
                                .setDate(info.date)
                                .build();
                proto.writeTo(stream);

                Assert.assertNull(
                        "Seed with missing fields should've failed to load.",
                        VariationsUtils.readSeedFile(file));
            } finally {
                if (stream != null) stream.close();
            }
        } finally {
            if (file != null) file.delete();
        }
    }

    // Test reading a seed that's been truncated at some arbitrary byte offsets, which should fail.
    @Test
    @MediumTest
    public void testReadTruncatedSeed() throws IOException {
        // Create a complete, serialized seed.
        SeedInfo info = VariationsTestUtils.createMockSeed();
        AwVariationsSeed proto =
                AwVariationsSeed.newBuilder()
                        .setSignature(info.signature)
                        .setCountry(info.country)
                        .setDate(info.date)
                        .setIsGzipCompressed(info.isGzipCompressed)
                        .setSeedData(ByteString.copyFrom(info.seedData))
                        .build();
        byte[] protoBytes = proto.toByteArray();

        // Sanity check: protoBytes is at least as long as the seedData field.
        Assert.assertTrue(protoBytes.length >= info.seedData.length);

        // Create slices of that seed in 10-byte increments.
        for (int offset = 10; offset < protoBytes.length; offset += 10) {
            byte[] slice = Arrays.copyOfRange(protoBytes, 0, offset);
            File file = null;
            try {
                file = File.createTempFile("seed", null, null);
                FileOutputStream stream = null;
                try {
                    stream = new FileOutputStream(file);
                    stream.write(slice);
                } finally {
                    if (stream != null) stream.close();
                }

                // Reading each truncated seed should fail.
                Assert.assertNull(
                        "Seed truncated from "
                                + protoBytes.length
                                + " to "
                                + offset
                                + " bytes should've failed to load.",
                        VariationsUtils.readSeedFile(file));
            } finally {
                if (file != null) file.delete();
            }
        }
    }
}
