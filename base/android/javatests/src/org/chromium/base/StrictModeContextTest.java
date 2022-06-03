// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.StrictMode;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

/**
 * Tests for the StrictModeContext class.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class StrictModeContextTest {
    private StrictMode.ThreadPolicy mOldThreadPolicy;
    private StrictMode.VmPolicy mOldVmPolicy;
    private FileOutputStream mFosForWriting;
    private FileInputStream mFisForReading;

    @Before
    public void setUp() throws Exception {
        mFosForWriting = new FileOutputStream(File.createTempFile("foo", "bar"));
        mFisForReading = new FileInputStream(File.createTempFile("foo", "baz"));
        enableStrictMode();
    }

    @After
    public void tearDown() throws Exception {
        disableStrictMode();
        mFosForWriting.close();
        mFisForReading.close();
    }

    private void enableStrictMode() {
        mOldThreadPolicy = StrictMode.getThreadPolicy();
        mOldVmPolicy = StrictMode.getVmPolicy();
        StrictMode.setThreadPolicy(new StrictMode.ThreadPolicy.Builder()
                                           .detectAll()
                                           .penaltyLog()
                                           .penaltyDeath()
                                           .build());
        StrictMode.setVmPolicy(
                new StrictMode.VmPolicy.Builder().detectAll().penaltyLog().penaltyDeath().build());
    }

    private void disableStrictMode() {
        StrictMode.setThreadPolicy(mOldThreadPolicy);
        StrictMode.setVmPolicy(mOldVmPolicy);
    }

    private void writeToDisk() {
        try {
            mFosForWriting.write(ApiCompatibilityUtils.getBytesUtf8("Foo"));
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private void assertWriteToDiskThrows() {
        boolean didThrow = false;
        try {
            writeToDisk();
        } catch (Exception e) {
            didThrow = true;
        }
        Assert.assertTrue("Expected disk write to  throw.", didThrow);
    }

    private void readFromDisk() {
        try {
            mFisForReading.read();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    private void assertReadFromDiskThrows() {
        boolean didThrow = false;
        try {
            readFromDisk();
        } catch (Exception e) {
            didThrow = true;
        }
        Assert.assertTrue("Expected disk read to  throw.", didThrow);
    }

    @Test
    @SmallTest
    public void testAllowDiskWrites() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            writeToDisk();
        }
        assertWriteToDiskThrows();
    }

    @Test
    @SmallTest
    public void testAllowDiskReads() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            readFromDisk();
            assertWriteToDiskThrows();
        }
        assertReadFromDiskThrows();
    }
}
