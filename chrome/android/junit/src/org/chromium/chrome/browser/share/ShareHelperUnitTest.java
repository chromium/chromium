// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowIntent;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.JUnitTestGURLs;

/**
 * Unit tests for static functions in {@link ShareHelper}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowIntent.class})
@Features.DisableFeatures(ChromeFeatureList.CHROME_SHARING_HUB_LAUNCH_ADJACENT)
public class ShareHelperUnitTest {
    private static final String IMAGE_URI = "file://path/to/image.png";
    private static final ComponentName TEST_COMPONENT_NAME =
            new ComponentName("test.package", "test.class.name");

    private WindowAndroid mWindow;
    private Activity mActivity;
    private Uri mImageUri;

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mWindow = new ActivityWindowAndroid(
                mActivity, false, IntentRequestTracker.createFromActivity(mActivity));
        mImageUri = Uri.parse(IMAGE_URI);
    }

    @After
    public void tearDown() {
        mWindow.destroy();
        mActivity.finish();
    }

    @Test
    public void shareImageWithChooser() {
        ShareHelper.shareImage(mWindow, null, null, mImageUri, JUnitTestGURLs.BLUE_1);

        Intent nextIntent = Shadows.shadowOf(mActivity).peekNextStartedActivity();
        assertNotNull("Shared intent is null.", nextIntent);
        assertEquals(
                "Intent is not a chooser intent.", Intent.ACTION_CHOOSER, nextIntent.getAction());

        // Verify sharing intent has the right image.
        Intent sharingIntent = nextIntent.getParcelableExtra(Intent.EXTRA_INTENT);
        assertEquals("Intent is not a SEND intent.", Intent.ACTION_SEND, sharingIntent.getAction());
        assertEquals("Text URL not set correctly.", JUnitTestGURLs.BLUE_1,
                sharingIntent.getStringExtra(Intent.EXTRA_TEXT));
        assertEquals("Image URI not set correctly.", mImageUri,
                sharingIntent.getParcelableExtra(Intent.EXTRA_STREAM));
        assertNotNull("Shared image does not have preview set.", sharingIntent.getClipData());
    }

    @Test
    public void shareImageWithComponentName() {
        ShareHelper.shareImage(
                mWindow, null, TEST_COMPONENT_NAME, mImageUri, JUnitTestGURLs.BLUE_1);

        Intent nextIntent = Shadows.shadowOf(mActivity).peekNextStartedActivity();
        assertNotNull("Shared intent is null.", nextIntent);
        assertEquals(
                "Next fired intent should be a SEND intent when direct sharing with component.",
                Intent.ACTION_SEND, nextIntent.getAction());
    }
}
