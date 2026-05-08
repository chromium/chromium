// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import static org.junit.Assert.assertTrue;

import android.app.Activity;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;

/** Unit tests for {@link LongScreenshotsUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowToast.class})
public class LongScreenshotsUtilsTest {
    private final Activity mActivity;

    public LongScreenshotsUtilsTest() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
    }

    @Test
    public void testShowErrorMessage() {
        LongScreenshotsUtils.showErrorMessage(mActivity);

        ShadowLooper.idleMainLooper();

        assertTrue(
                "Toast was not shown",
                ShadowToast.showedCustomToast(
                        mActivity.getString(R.string.sharing_long_screenshot_unknown_error),
                        R.id.toast_text));
    }
}
