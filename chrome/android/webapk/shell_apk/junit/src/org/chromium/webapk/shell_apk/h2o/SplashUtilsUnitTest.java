// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk.h2o;

import static org.robolectric.Robolectric.setupActivity;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Unit tests for {@link SplashUtils}. */
@RunWith(RobolectricTestRunner.class)
public final class SplashUtilsUnitTest {
    /**
     * Test that SplashUtils sets the correct dark background color when the system is in night
     * mode.
     */
    @Test
    @Config(qualifiers = "night")
    public void testSplashScreenBackgroundWhenNightMode() {
        Activity testActivity = setupActivity(Activity.class);
        View view = SplashUtils.createSplashView(testActivity);

        ColorDrawable background = (ColorDrawable) view.getBackground();

        Assert.assertEquals(Color.parseColor("#202020"), background.getColor());
    }
}
