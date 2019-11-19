// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.graphics.drawable.BitmapDrawable;
import android.support.test.filters.SmallTest;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests for splash screens with EXTRA_ICON specified in the Intent.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebappSplashScreenHomescreenIconTest {
    @Rule
    public final WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();

    private ViewGroup mSplashScreen;

    @Before
    public void setUp() {
        mSplashScreen = mActivityTestRule.startWebappActivityAndWaitForSplashScreen(
                mActivityTestRule.createIntent().putExtra(
                        ShortcutHelper.EXTRA_ICON, WebappActivityTestRule.TEST_ICON));
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testShowFallbackIcon() {
        ImageView splashImage =
                (ImageView) mSplashScreen.findViewById(R.id.webapp_splash_screen_icon);
        BitmapDrawable drawable = (BitmapDrawable) splashImage.getDrawable();

        Assert.assertEquals(192, drawable.getBitmap().getWidth());
        Assert.assertEquals(192, drawable.getBitmap().getHeight());
    }
}
