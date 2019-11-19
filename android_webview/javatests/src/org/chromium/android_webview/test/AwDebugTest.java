// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertNotEquals;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.SINGLE_PROCESS;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwDebug;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.Scanner;

/**
 * A test suite for AwDebug class.
 */
@RunWith(AwJUnit4ClassRunner.class)
@OnlyRunIn(SINGLE_PROCESS) // Only works in single-process mode, http://crbug.com/568825.
public class AwDebugTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private static final String TAG = "AwDebugTest";

    // These constants must match android_webview/browser/aw_debug.cc.
    private static final String WHITELISTED_DEBUG_KEY = "AW_WHITELISTED_DEBUG_KEY";
    private static final String NON_WHITELISTED_DEBUG_KEY = "AW_NONWHITELISTED_DEBUG_KEY";
    private static final String DEBUG_VALUE = "AW_DEBUG_VALUE";

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Debug"})
    @DisabledTest(message = "crbug.com/913515")
    public void testDump() throws Throwable {
        File f = File.createTempFile("dump", ".dmp");
        try {
            Assert.assertTrue(AwDebug.dumpWithoutCrashing(f));
            Assert.assertTrue(f.canRead());
            assertNotEquals(f.length(), 0);
        } finally {
            Assert.assertTrue(f.delete());
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Debug"})
    @DisabledTest(message = "crbug.com/913515")
    public void testDumpContainsWhitelistedKey() throws Throwable {
        File f = File.createTempFile("dump", ".dmp");
        try {
            AwDebug.initCrashKeysForTesting();
            AwDebug.setWhiteListedKeyForTesting();
            Assert.assertTrue(AwDebug.dumpWithoutCrashing(f));
            assertContainsCrashKeyValue(f, WHITELISTED_DEBUG_KEY, DEBUG_VALUE);
        } finally {
            Assert.assertTrue(f.delete());
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Debug"})
    @DisabledTest(message = "crbug.com/913515")
    public void testDumpDoesNotContainNonWhitelistedKey() throws Throwable {
        File f = File.createTempFile("dump", ".dmp");
        try {
            AwDebug.initCrashKeysForTesting();
            AwDebug.setNonWhiteListedKeyForTesting();
            Assert.assertTrue(AwDebug.dumpWithoutCrashing(f));
            assertNotContainsCrashKeyValue(f, NON_WHITELISTED_DEBUG_KEY);
        } finally {
            Assert.assertTrue(f.delete());
        }
    }

    private void assertNotContainsCrashKeyValue(File dump, String key) throws IOException {
        String dumpContents = readEntireFile(dump);
        // We are expecting the following to NOT be in the dumpContents:
        //
        // Content-Disposition: form-data; name="AW_DEBUG_KEY"
        // AW_DEBUG_VALUE
        Assert.assertFalse(dumpContents.contains(getDebugKeyLine(key)));
    }

    private void assertContainsCrashKeyValue(File dump, String key, String expectedValue)
            throws IOException {
        String dumpContents = readEntireFile(dump);
        // We are expecting the following lines:
        //
        // Content-Disposition: form-data; name="AW_DEBUG_KEY"
        // AW_DEBUG_VALUE
        String debugKeyLine = getDebugKeyLine(key);
        Assert.assertTrue(dumpContents.contains(debugKeyLine));

        int debugKeyIndex = dumpContents.indexOf(debugKeyLine);
        // Read the word after the line containing the debug key
        Scanner debugValueScanner =
                new Scanner(dumpContents.substring(debugKeyIndex + debugKeyLine.length()));
        Assert.assertTrue(debugValueScanner.hasNext());
        Assert.assertEquals(expectedValue, debugValueScanner.next());
    }

    private static String getDebugKeyLine(String debugKey) {
        return "Content-Disposition: form-data; name=\"" + debugKey + "\"";
    }

    private static String readEntireFile(File file) throws IOException {
        FileInputStream fileInputStream = new FileInputStream(file);
        try {
            byte[] data = new byte[(int) file.length()];
            fileInputStream.read(data);
            return new String(data);
        } finally {
            fileInputStream.close();
        }
    }
}
