// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;

import android.app.Activity;
import android.graphics.Color;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.base.TestActivity;

@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class AuthTabColorProviderUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenario =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;
    private AuthTabColorProvider mAuthTabColorProvider;

    @Before
    public void setUp() {
        mActivityScenario.getScenario().onActivity(activity -> mActivity = activity);
        mAuthTabColorProvider = new AuthTabColorProvider(mActivity);
    }

    @Test
    public void testReturnsDefaults() {
        int color = ChromeColors.getDefaultThemeColor(mActivity, false);
        assertEquals("Wrong toolbar color", color, mAuthTabColorProvider.getToolbarColor());
        assertEquals("Wrong bottom bar color", color, mAuthTabColorProvider.getBottomBarColor());
        assertEquals(
                "Initial bg color should be transparent.",
                Color.TRANSPARENT,
                mAuthTabColorProvider.getInitialBackgroundColor());
        assertFalse(
                "Shouldn't have a custom toolbar color specified.",
                mAuthTabColorProvider.hasCustomToolbarColor());
        assertNull(
                "Shouldn't have a navbar color specified.",
                mAuthTabColorProvider.getNavigationBarColor());
        assertNull(
                "Shouldn't have a navbar divider color specified.",
                mAuthTabColorProvider.getNavigationBarDividerColor());
    }
}
