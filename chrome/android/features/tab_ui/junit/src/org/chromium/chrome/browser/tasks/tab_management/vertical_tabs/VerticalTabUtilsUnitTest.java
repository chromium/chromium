// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Unit tests for {@link VerticalTabUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.VERTICAL_TABS_ENABLED);
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testIsVerticalTabsEligible_FeatureDisabled() {
        FeatureOverrides.disable(ChromeFeatureList.ANDROID_VERTICAL_TABS);
        assertFalse(VerticalTabUtils.isVerticalTabsEligible(mContext));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw400dp")
    public void testIsVerticalTabsEligible_NotTablet() {
        FeatureOverrides.enable(ChromeFeatureList.ANDROID_VERTICAL_TABS);
        assertFalse(VerticalTabUtils.isVerticalTabsEligible(mContext));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testIsVerticalTabsEligible_Eligible() {
        FeatureOverrides.enable(ChromeFeatureList.ANDROID_VERTICAL_TABS);
        assertTrue(VerticalTabUtils.isVerticalTabsEligible(mContext));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testIsVerticalTabsEnabled_FalseWhenNotEligible() {
        FeatureOverrides.disable(ChromeFeatureList.ANDROID_VERTICAL_TABS);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, true);
        assertFalse(VerticalTabUtils.isVerticalTabsEnabled(mContext));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testIsVerticalTabsEnabled_FalseWhenPreferenceDisabled() {
        FeatureOverrides.enable(ChromeFeatureList.ANDROID_VERTICAL_TABS);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false);
        assertFalse(VerticalTabUtils.isVerticalTabsEnabled(mContext));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testIsVerticalTabsEnabled_Enabled() {
        FeatureOverrides.enable(ChromeFeatureList.ANDROID_VERTICAL_TABS);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, true);
        assertTrue(VerticalTabUtils.isVerticalTabsEnabled(mContext));
    }
}
