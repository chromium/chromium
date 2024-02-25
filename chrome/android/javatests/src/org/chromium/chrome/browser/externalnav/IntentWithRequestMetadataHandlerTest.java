// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.content.Intent;
import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.externalnav.IntentWithRequestMetadataHandler.RequestMetadata;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/** Instrumentation tests for {@link IntentWithRequestMetadataHandler}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class IntentWithRequestMetadataHandlerTest {
    @After
    public void tearDown() {
        IntentWithRequestMetadataHandler.getInstance().clear();
    }

    @Test
    @SmallTest
    public void testCanUseRequestMetadataTokenOnlyOnce() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("content://abc"));
        IntentWithRequestMetadataHandler.getInstance()
                .onNewIntentWithRequestMetadata(intent, new RequestMetadata(true, true));
        Assert.assertTrue(
                intent.hasExtra(IntentWithRequestMetadataHandler.EXTRA_REQUEST_METADATA_TOKEN));
        RequestMetadata metadata =
                IntentWithRequestMetadataHandler.getInstance().getRequestMetadataAndClear(intent);
        Assert.assertTrue(metadata.hasUserGesture());
        Assert.assertTrue(metadata.isRendererInitiated());
        Assert.assertNull(
                IntentWithRequestMetadataHandler.getInstance().getRequestMetadataAndClear(intent));
    }

    @Test
    @SmallTest
    public void testModifiedRequestMetadataToken() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("content://abc"));
        IntentWithRequestMetadataHandler.getInstance()
                .onNewIntentWithRequestMetadata(intent, new RequestMetadata(true, true));
        intent.setData(Uri.parse("content://xyz"));
        Assert.assertNull(
                IntentWithRequestMetadataHandler.getInstance().getRequestMetadataAndClear(intent));
    }

    @Test
    @SmallTest
    public void testPreviousRequestMetadataToken() {
        Intent intent1 = new Intent(Intent.ACTION_VIEW, Uri.parse("content://abc"));
        IntentWithRequestMetadataHandler.getInstance()
                .onNewIntentWithRequestMetadata(intent1, new RequestMetadata(true, true));
        Intent intent2 = new Intent(Intent.ACTION_VIEW, Uri.parse("content://xyz"));
        IntentWithRequestMetadataHandler.getInstance()
                .onNewIntentWithRequestMetadata(intent2, new RequestMetadata(true, false));
        Assert.assertNull(
                IntentWithRequestMetadataHandler.getInstance().getRequestMetadataAndClear(intent1));
    }
}
