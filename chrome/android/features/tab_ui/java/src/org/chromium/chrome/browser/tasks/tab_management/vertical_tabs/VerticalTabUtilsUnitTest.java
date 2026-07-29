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
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;

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
        VerticalTabUtils.setVerticalTabsEnabled(true);
        assertFalse(VerticalTabUtils.isVerticalTabsEnabled(mContext));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testIsVerticalTabsEnabled_FalseWhenPreferenceDisabled() {
        FeatureOverrides.enable(ChromeFeatureList.ANDROID_VERTICAL_TABS);
        VerticalTabUtils.setVerticalTabsEnabled(false);
        assertFalse(VerticalTabUtils.isVerticalTabsEnabled(mContext));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testIsVerticalTabsEnabled_Enabled() {
        FeatureOverrides.enable(ChromeFeatureList.ANDROID_VERTICAL_TABS);
        VerticalTabUtils.setVerticalTabsEnabled(true);
        assertTrue(VerticalTabUtils.isVerticalTabsEnabled(mContext));
    }

    @Test
    @SmallTest
    public void testIsExpandOnHoverEnabled() {
        assertFalse(VerticalTabUtils.isExpandOnHoverEnabled());

        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS, "expand_on_hover", true);
        assertTrue(VerticalTabUtils.isExpandOnHoverEnabled());
    }

    @Test
    @SmallTest
    public void testRecordLayoutToggle_Enable_AppMenu() {
        assertLayoutToggleHistogram(
                VerticalTabUtils.LayoutSwitchEntryPoint.APP_MENU,
                /* isEnabling= */ true,
                VerticalTabUtils.LayoutToggleSourceAndDirection.ENABLE_APP_MENU);
    }

    @Test
    @SmallTest
    public void testRecordLayoutToggle_Enable_TabContextMenu() {
        assertLayoutToggleHistogram(
                VerticalTabUtils.LayoutSwitchEntryPoint.TAB_CONTEXT_MENU,
                /* isEnabling= */ true,
                VerticalTabUtils.LayoutToggleSourceAndDirection.ENABLE_TAB_CONTEXT_MENU);
    }

    @Test
    @SmallTest
    public void testRecordLayoutToggle_Enable_TabStripContextMenu() {
        assertLayoutToggleHistogram(
                VerticalTabUtils.LayoutSwitchEntryPoint.TAB_STRIP_CONTEXT_MENU,
                /* isEnabling= */ true,
                VerticalTabUtils.LayoutToggleSourceAndDirection.ENABLE_TAB_STRIP_CONTEXT_MENU);
    }

    @Test
    @SmallTest
    public void testRecordLayoutToggle_Disable_AppMenu() {
        assertLayoutToggleHistogram(
                VerticalTabUtils.LayoutSwitchEntryPoint.APP_MENU,
                /* isEnabling= */ false,
                VerticalTabUtils.LayoutToggleSourceAndDirection.DISABLE_APP_MENU);
    }

    @Test
    @SmallTest
    public void testRecordLayoutToggle_Disable_TabContextMenu() {
        assertLayoutToggleHistogram(
                VerticalTabUtils.LayoutSwitchEntryPoint.TAB_CONTEXT_MENU,
                /* isEnabling= */ false,
                VerticalTabUtils.LayoutToggleSourceAndDirection.DISABLE_TAB_CONTEXT_MENU);
    }

    @Test
    @SmallTest
    public void testRecordLayoutToggle_Disable_TabStripContextMenu() {
        assertLayoutToggleHistogram(
                VerticalTabUtils.LayoutSwitchEntryPoint.TAB_STRIP_CONTEXT_MENU,
                /* isEnabling= */ false,
                VerticalTabUtils.LayoutToggleSourceAndDirection.DISABLE_TAB_STRIP_CONTEXT_MENU);
    }

    private void assertLayoutToggleHistogram(
            @VerticalTabUtils.LayoutSwitchEntryPoint int entryPoint,
            boolean isEnabling,
            @VerticalTabUtils.LayoutToggleSourceAndDirection int expectedEnumVal) {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.VerticalTabs.LayoutToggleSourceAndDirection",
                                expectedEnumVal)
                        .build();

        VerticalTabUtils.recordLayoutToggle(entryPoint, isEnabling);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testIsVerticalTabsEnabled_DefaultFalseWhenEligible() {
        FeatureOverrides.enable(ChromeFeatureList.ANDROID_VERTICAL_TABS);
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS,
                VerticalTabUtils.ENABLE_BY_DEFAULT_PARAM,
                false);

        // Preference is not set, should default to false.
        assertFalse(VerticalTabUtils.isVerticalTabsEnabled(mContext));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testIsVerticalTabsEnabled_DefaultTrueWhenEligible() {
        FeatureOverrides.enable(ChromeFeatureList.ANDROID_VERTICAL_TABS);
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS,
                VerticalTabUtils.ENABLE_BY_DEFAULT_PARAM,
                true);

        // Preference is not set, should default to true when eligible.
        assertTrue(VerticalTabUtils.isVerticalTabsEnabled(mContext));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testIsVerticalTabsEnabled_RespectsPrefOverDefault() {
        FeatureOverrides.enable(ChromeFeatureList.ANDROID_VERTICAL_TABS);
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS,
                VerticalTabUtils.ENABLE_BY_DEFAULT_PARAM,
                true);

        // Explicitly set preference to false.
        VerticalTabUtils.setVerticalTabsEnabled(false);

        // Should respect preference (false) over default (true).
        assertFalse(VerticalTabUtils.isVerticalTabsEnabled(mContext));

        // Explicitly set preference to true.
        VerticalTabUtils.setVerticalTabsEnabled(true);

        // Should respect preference (true).
        assertTrue(VerticalTabUtils.isVerticalTabsEnabled(mContext));
    }
}
