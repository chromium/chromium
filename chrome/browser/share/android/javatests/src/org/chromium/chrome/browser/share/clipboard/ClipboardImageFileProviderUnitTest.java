// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.clipboard;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.CLIPBOARD_SHARED_URI;
import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.CLIPBOARD_SHARED_URI_TIMESTAMP;

import android.net.Uri;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.ui.base.Clipboard.ImageFileProvider.ClipboardFileMetadata;

/**
 * Tests for ClipboardImageFileProvider.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ClipboardImageFileProviderUnitTest {
    private static final Uri CONTENT_URI = Uri.parse("content://package/path/image.png");

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    ClipboardImageFileProvider mClipboardImageFileProvider;
    SharedPreferencesManager mSharedPreferencesManager;

    @Before
    public void setUp() throws Exception {
        mClipboardImageFileProvider = new ClipboardImageFileProvider();
    }

    @After
    public void tearDown() {
        mClipboardImageFileProvider.clearLastCopiedImageMetadata();
    }

    @Test
    @Feature("ClipboardImageFileProvider")
    public void testStoreLastCopiedImageMetadata() {
        long timestamp = System.currentTimeMillis();
        mClipboardImageFileProvider.storeLastCopiedImageMetadata(
                new ClipboardFileMetadata(CONTENT_URI, timestamp));

        String uriString =
                SharedPreferencesManager.getInstance().readString(CLIPBOARD_SHARED_URI, null);
        long timestampActual =
                SharedPreferencesManager.getInstance().readLong(CLIPBOARD_SHARED_URI_TIMESTAMP);
        Assert.assertEquals(CONTENT_URI.toString(), uriString);
        Assert.assertEquals(timestamp, timestampActual);
    }

    @Test
    @Feature("ClipboardImageFileProvider")
    public void testGetLastCopiedImageMetadata() {
        long timestamp = System.currentTimeMillis();
        mClipboardImageFileProvider.storeLastCopiedImageMetadata(
                new ClipboardFileMetadata(CONTENT_URI, timestamp));

        ClipboardFileMetadata metadata = mClipboardImageFileProvider.getLastCopiedImageMetadata();
        Assert.assertTrue(CONTENT_URI.equals(metadata.uri));
        Assert.assertEquals(timestamp, metadata.timestamp);
    }

    @Test
    @Feature("ClipboardImageFileProvider")
    public void testClearLastCopiedImageMetadata() {
        long timestamp = System.currentTimeMillis();
        mClipboardImageFileProvider.storeLastCopiedImageMetadata(
                new ClipboardFileMetadata(CONTENT_URI, timestamp));

        Assert.assertTrue(SharedPreferencesManager.getInstance().contains(CLIPBOARD_SHARED_URI));
        Assert.assertTrue(
                SharedPreferencesManager.getInstance().contains(CLIPBOARD_SHARED_URI_TIMESTAMP));

        mClipboardImageFileProvider.clearLastCopiedImageMetadata();
        Assert.assertFalse(SharedPreferencesManager.getInstance().contains(CLIPBOARD_SHARED_URI));
        Assert.assertFalse(
                SharedPreferencesManager.getInstance().contains(CLIPBOARD_SHARED_URI_TIMESTAMP));
    }
}