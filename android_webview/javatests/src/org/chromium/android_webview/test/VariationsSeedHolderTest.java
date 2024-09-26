// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.os.ParcelFileDescriptor;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.VariationsFastFetchModeUtils;
import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.android_webview.services.VariationsSeedHolder;
import org.chromium.android_webview.test.util.VariationsTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.components.variations.firstrun.VariationsSeedFetcher.SeedInfo;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Date;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Test VariationsSeedHolder. */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
public class VariationsSeedHolderTest {
    private static class TestHolder extends VariationsSeedHolder {
        private final CallbackHelper mWriteFinished; // notified after each writeSeedIfNewer
        private final CallbackHelper mUpdateFinished; // notified after each updateSeed

        public TestHolder() {
            mWriteFinished = new CallbackHelper();
            mUpdateFinished = new CallbackHelper();
        }

        public TestHolder(CallbackHelper writeFinished) {
            mWriteFinished = writeFinished;
            mUpdateFinished = new CallbackHelper();
        }

        // Don't schedule the seed download job.
        @Override
        public void scheduleFetchIfNeeded() {}

        @Override
        public void onWriteFinished() {
            mWriteFinished.notifyCalled();
        }

        public void writeSeedIfNewerBlocking(File destination, long date)
                throws IOException, TimeoutException {
            ParcelFileDescriptor fd = null;
            try {
                fd = ParcelFileDescriptor.open(destination, ParcelFileDescriptor.MODE_WRITE_ONLY);
                int calls = mWriteFinished.getCallCount();
                writeSeedIfNewer(fd, date);
                mWriteFinished.waitForCallback(calls);
            } finally {
                if (fd != null) fd.close();
            }
        }

        public void updateSeedBlocking(SeedInfo newSeed) throws TimeoutException {
            int calls = mUpdateFinished.getCallCount();
            updateSeed(newSeed, /* onFinished= */ () -> mUpdateFinished.notifyCalled());
            mUpdateFinished.waitForCallback(calls);
        }
    }

    @Before
    public void setUp() throws IOException {
        VariationsTestUtils.deleteSeeds();
    }

    @After
    public void tearDown() throws IOException {
        VariationsTestUtils.deleteSeeds();
    }

    // Request that the seed holder write its current seed to a file when the holder has no seed. No
    // write should happen.
    @Test
    @MediumTest
    public void testWriteNoSeed() throws IOException, TimeoutException {
        TestHolder holder = new TestHolder();
        File file = null;
        try {
            file = File.createTempFile("seed", null, null);
            holder.writeSeedIfNewerBlocking(file, Long.MIN_VALUE);
            Assert.assertEquals(0L, file.length());
        } finally {
            if (file != null) file.delete();
        }
    }

    // Test updating the holder with the mock seed, and then request the holder writes that seed to
    // an empty file. The written seed should match the mock seed.
    @Test
    @MediumTest
    public void testUpdateAndWriteToEmptySeed() throws IOException, TimeoutException {
        try {
            TestHolder holder = new TestHolder();
            holder.updateSeedBlocking(VariationsTestUtils.createMockSeed());
            File file = null;
            try {
                file = File.createTempFile("seed", null, null);
                holder.writeSeedIfNewerBlocking(file, Long.MIN_VALUE);
                SeedInfo readSeed = VariationsUtils.readSeedFile(file);
                VariationsTestUtils.assertSeedsEqual(
                        VariationsTestUtils.createMockSeed(), readSeed);
            } finally {
                if (file != null) file.delete();
            }
        } finally {
            VariationsTestUtils.deleteSeeds(); // Remove the holder's saved seed.
        }
    }

    // Test updating the holder with the mock seed, and then request the holder writes that seed to
    // a file. Pretend the file already contains a seed, but it's out of date, so writing should
    // proceed. The written seed should match the mock seed.
    @Test
    @MediumTest
    public void testUpdateAndWriteToStaleSeed() throws IOException, TimeoutException {
        try {
            SeedInfo mockSeed = VariationsTestUtils.createMockSeed();
            long mockDateMinusOneDay = mockSeed.date - TimeUnit.DAYS.toMillis(1);
            TestHolder holder = new TestHolder();
            holder.updateSeedBlocking(mockSeed);
            File file = null;
            try {
                file = File.createTempFile("seed", null, null);
                holder.writeSeedIfNewerBlocking(file, mockDateMinusOneDay);
                SeedInfo readSeed = VariationsUtils.readSeedFile(file);
                VariationsTestUtils.assertSeedsEqual(mockSeed, readSeed);
            } finally {
                if (file != null) file.delete();
            }
        } finally {
            VariationsTestUtils.deleteSeeds(); // Remove the holder's saved seed.
        }
    }

    // Test updating the holder with the mock seed, and then request the holder writes that seed to
    // a file. Pretend the file already contains an up-to-date seed, so no write should happen.
    @Test
    @MediumTest
    public void testUpdateAndWriteToFreshSeed() throws IOException, TimeoutException {
        try {
            SeedInfo mockSeed = VariationsTestUtils.createMockSeed();
            TestHolder holder = new TestHolder();
            holder.updateSeedBlocking(mockSeed);
            File file = null;
            try {
                file = File.createTempFile("seed", null, null);
                holder.writeSeedIfNewerBlocking(file, mockSeed.date);
                Assert.assertEquals(0L, file.length());
            } finally {
                if (file != null) file.delete();
            }
        } finally {
            VariationsTestUtils.deleteSeeds(); // Remove the holder's saved seed.
        }
    }

    @Test
    @MediumTest
    public void testConcurrentUpdatesAndWrites()
            throws IOException, FileNotFoundException, TimeoutException {
        ArrayList<File> files = new ArrayList<>();
        try {
            ArrayList<ParcelFileDescriptor> fds = new ArrayList<>();
            try {
                // Create a series of mock seeds (5 chosen arbitrarily) which will be "downloaded"
                // to the holder via updateSeed. Differentiate the seeds by filling each seedData
                // field with the index of the seed.
                SeedInfo[] mockSeeds = new SeedInfo[5];
                for (int i = 0; i < mockSeeds.length; i++) {
                    mockSeeds[i] = VariationsTestUtils.createMockSeed();
                    mockSeeds[i].seedData = new byte[100];
                    Arrays.fill(mockSeeds[i].seedData, (byte) i);
                }

                // Used to track the completion of every updateSeed and writeSeedIfNewer call.
                CallbackHelper callbackHelper = new CallbackHelper();
                int callbacksExpected = 0;

                // TestHolder will notify callbackHelper whenever a writeSeedIfNewer request
                // completes.
                TestHolder holder = new TestHolder(callbackHelper);

                // "Download" each mock seed to the holder.
                for (int i = 0; i < mockSeeds.length; i++) {
                    callbacksExpected++;
                    holder.updateSeed(
                            mockSeeds[i], /* onFinished= */ () -> callbackHelper.notifyCalled());

                    // Between each "download", schedule a few (3 chosen arbitrarily) requests for
                    // the seed, creating a new file to receive each request.
                    for (int write = 0; write < 3; write++) {
                        File file = File.createTempFile("seed", null, null);
                        files.add(file);

                        ParcelFileDescriptor fd =
                                ParcelFileDescriptor.open(
                                        file, ParcelFileDescriptor.MODE_WRITE_ONLY);
                        fds.add(fd);

                        callbacksExpected++;
                        holder.writeSeedIfNewer(fd, Long.MIN_VALUE);
                    }
                }

                // Wait for all updateSeed and writeSeedIfNewer calls to finish.
                callbackHelper.waitForCallback(0, callbacksExpected);

                // Read each requested seed and ensure it's either empty (if the request was
                // scheduled before any "downloads") or it matches one of the mock seeds. For the
                // requests that got a mock seed, there's no guarantee as to which seed they got.
                for (int i = 0; i < files.size(); i++) {
                    if (files.get(i).length() == 0) continue;

                    SeedInfo readSeed = VariationsUtils.readSeedFile(files.get(i));
                    Assert.assertNotNull("Failed reading seed index " + i, readSeed);

                    boolean match = false;
                    for (SeedInfo mockSeed : mockSeeds) {
                        if (Arrays.equals(readSeed.seedData, mockSeed.seedData)) {
                            match = true;
                            break;
                        }
                    }
                    Assert.assertTrue(
                            "Seed data "
                                    + Arrays.toString(readSeed.seedData)
                                    + " read from seed index "
                                    + i
                                    + " does not match any written data",
                            match);
                }
            } finally {
                for (ParcelFileDescriptor fd : fds) {
                    fd.close();
                }
            }
        } finally {
            for (File file : files) {
                if (!file.delete()) {
                    throw new IOException("Failed to delete " + file);
                }
            }
            VariationsTestUtils.deleteSeeds(); // Remove the holder's saved seed.
        }
    }

    @Test
    @MediumTest
    public void testSeedFileUpdateMarkedAsCompletedWithNewlyUpdatedTimestamp() throws IOException {
        // With no variations seed recently fetched, the seed fetch completion decision should fall
        // to the timestamp of the seed file.
        long startingTime = 54000L;

        final Date date = mock(Date.class);
        when(date.getTime())
                .thenReturn(
                        startingTime + VariationsFastFetchModeUtils.MAX_ALLOWABLE_SEED_AGE_MS - 1L);
        VariationsSeedHolder.getInstance().setDateForTesting(date);
        File seedFile = VariationsUtils.getSeedFile();
        try {
            Assert.assertFalse("Stamp file already exists", seedFile.exists());
            Assert.assertTrue("Failed to create stamp file", seedFile.createNewFile());
            Assert.assertTrue("Failed to set stamp time", seedFile.setLastModified(startingTime));
            Assert.assertTrue(
                    "Seed fetch should be marked as completed since the "
                            + "seed timestamp was just updated",
                    VariationsSeedHolder.getInstance().isSeedFileFresh());
        } finally {
            VariationsTestUtils.deleteSeeds(); // Remove the stamp file.
        }
    }

    @Test
    @MediumTest
    public void testSeedFileUpdateMarkedAsNotCompletedWithOutOfDateTimestamp() throws IOException {
        long startingTime = 54000L;

        final Date date = mock(Date.class);
        when(date.getTime())
                .thenReturn(
                        startingTime + VariationsFastFetchModeUtils.MAX_ALLOWABLE_SEED_AGE_MS + 1L);
        VariationsSeedHolder.getInstance().setDateForTesting(date);
        File seedFile = VariationsUtils.getSeedFile();
        try {
            Assert.assertFalse("Stamp file already exists", seedFile.exists());
            Assert.assertTrue("Failed to create stamp file", seedFile.createNewFile());
            Assert.assertTrue("Failed to set stamp time", seedFile.setLastModified(startingTime));

            // With no variations seed recently fetched, the seed fetch completion decision should
            // fall to the timestamp of the seed file.
            Assert.assertFalse(
                    "Seed fetch should not be marked as completed since the "
                            + "seed timestamp was set to larger than the ",
                    VariationsSeedHolder.getInstance().isSeedFileFresh());
        } finally {
            VariationsTestUtils.deleteSeeds(); // Remove the stamp file.
        }
    }

    @Test
    @MediumTest
    public void testSeedFileUpdateMarkedAsNotCompletedWithOutOfDateTimestampWithLowStamp()
            throws IOException {
        // Note: setLastModified has a second's precision. Since there is millisecond precision in
        // this, the three least significant digits are truncated when setting the timestamp.
        long startingTime = 1000L;

        final Date date = mock(Date.class);
        when(date.getTime())
                .thenReturn(
                        startingTime + VariationsFastFetchModeUtils.MAX_ALLOWABLE_SEED_AGE_MS + 1L);
        VariationsSeedHolder.getInstance().setDateForTesting(date);
        File stamp = VariationsUtils.getStampFile();
        try {
            Assert.assertFalse("Stamp file already exists", stamp.exists());
            Assert.assertTrue("Failed to create stamp file", stamp.createNewFile());
            Assert.assertTrue("Failed to set stamp time", stamp.setLastModified(startingTime));

            // With no variations seed recently fetched, the seed fetch completion decision should
            // fall to the timestamp of the seed file.
            Assert.assertFalse(
                    "Seed fetch should not be marked as completed since the "
                            + "seed timestamp was set to larger than the ",
                    VariationsSeedHolder.getInstance().isSeedFileFresh());
        } finally {
            VariationsTestUtils.deleteSeeds(); // Remove the stamp file.
        }
    }
}
