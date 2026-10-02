// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.net.Uri;

import org.junit.After;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.externalnav.IntentWithRequestMetadataHandler.RequestMetadata;

/** Unit tests for {@link IntentWithRequestMetadataHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class IntentWithRequestMetadataHandlerTest {
    @After
    public void tearDown() {
        IntentWithRequestMetadataHandler.getInstance().clear();
    }

    @Test
    public void testCanUseRequestMetadataTokenOnlyOnce() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("content://abc"));
        IntentWithRequestMetadataHandler.getInstance()
                .onNewIntentWithRequestMetadata(intent, new RequestMetadata(true, true));
        assertTrue(intent.hasExtra(IntentWithRequestMetadataHandler.EXTRA_REQUEST_METADATA_TOKEN));
        RequestMetadata metadata =
                IntentWithRequestMetadataHandler.getInstance().getRequestMetadataAndClear(intent);
        assertTrue(metadata.hasUserGesture());
        assertTrue(metadata.isRendererInitiated());
        assertNull(
                IntentWithRequestMetadataHandler.getInstance().getRequestMetadataAndClear(intent));
    }

    @Test
    public void testModifiedRequestMetadataToken() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("content://abc"));
        IntentWithRequestMetadataHandler.getInstance()
                .onNewIntentWithRequestMetadata(intent, new RequestMetadata(true, true));
        intent.setData(Uri.parse("content://xyz"));
        assertNull(
                IntentWithRequestMetadataHandler.getInstance().getRequestMetadataAndClear(intent));
    }

    @Test
    public void testPreviousRequestMetadataToken() {
        Intent intent1 = new Intent(Intent.ACTION_VIEW, Uri.parse("content://abc"));
        IntentWithRequestMetadataHandler.getInstance()
                .onNewIntentWithRequestMetadata(intent1, new RequestMetadata(true, true));
        Intent intent2 = new Intent(Intent.ACTION_VIEW, Uri.parse("content://xyz"));
        IntentWithRequestMetadataHandler.getInstance()
                .onNewIntentWithRequestMetadata(intent2, new RequestMetadata(true, false));
        assertNull(
                IntentWithRequestMetadataHandler.getInstance().getRequestMetadataAndClear(intent1));
    }
}
