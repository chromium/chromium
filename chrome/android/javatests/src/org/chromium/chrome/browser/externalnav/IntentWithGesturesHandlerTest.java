// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.externalnav;

import android.content.Intent;
import android.net.Uri;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Instrumentation tests for {@link IntentWithGesturesHandler}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class IntentWithGesturesHandlerTest {
    @After
    public void tearDown() {
        IntentWithGesturesHandler.getInstance().clear();
    }

    @Test
    @SmallTest
    public void testCanUseGestureTokenOnlyOnce() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("content://abc"));
        IntentWithGesturesHandler.getInstance().onNewIntentWithGesture(intent);
        Assert.assertTrue(intent.hasExtra(IntentWithGesturesHandler.EXTRA_USER_GESTURE_TOKEN));
        Assert.assertTrue(IntentWithGesturesHandler.getInstance().getUserGestureAndClear(intent));
        Assert.assertFalse(IntentWithGesturesHandler.getInstance().getUserGestureAndClear(intent));
    }

    @Test
    @SmallTest
    public void testModifiedGestureToken() {
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse("content://abc"));
        IntentWithGesturesHandler.getInstance().onNewIntentWithGesture(intent);
        intent.setData(Uri.parse("content://xyz"));
        Assert.assertFalse(IntentWithGesturesHandler.getInstance().getUserGestureAndClear(intent));
    }

    @Test
    @SmallTest
    public void testPreviousGestureToken() {
        Intent intent1 = new Intent(Intent.ACTION_VIEW, Uri.parse("content://abc"));
        IntentWithGesturesHandler.getInstance().onNewIntentWithGesture(intent1);
        Intent intent2 = new Intent(Intent.ACTION_VIEW, Uri.parse("content://xyz"));
        IntentWithGesturesHandler.getInstance().onNewIntentWithGesture(intent2);
        Assert.assertFalse(IntentWithGesturesHandler.getInstance().getUserGestureAndClear(intent1));
    }
}
