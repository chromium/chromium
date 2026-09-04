// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.DeviceInfo;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils.LayoutSwitchEntryPoint;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils.LayoutToggleSourceAndDirection;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils.WindowWidthBoundary;

/** Unit tests for {@link VerticalTabUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class VerticalTabUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Context mMockContext;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @After
    public void tearDown() {
        DeviceInfo.resetIsDesktopForTesting();
        VerticalTabUtils.resetSharedPrefsForTesting();
    }

    @Test
    @SmallTest
    public void testVerticalTabRailCollapsedPreference() {
        assertFalse(VerticalTabUtils.isRailCollapsedFromSharedPref());

        VerticalTabUtils.setRailCollapsedInSharedPref(true);
        assertTrue(VerticalTabUtils.isRailCollapsedFromSharedPref());

        VerticalTabUtils.setRailCollapsedInSharedPref(false);
        assertFalse(VerticalTabUtils.isRailCollapsedFromSharedPref());
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
    public void testIsVerticalTabsEligible_NullContext() {
        FeatureOverrides.enable(ChromeFeatureList.ANDROID_VERTICAL_TABS);
        assertFalse(VerticalTabUtils.isVerticalTabsEligible(null));
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
    public void testIsVerticalTabsEnabled_NullContext() {
        FeatureOverrides.enable(ChromeFeatureList.ANDROID_VERTICAL_TABS);
        VerticalTabUtils.setVerticalTabsEnabled(true);
        assertFalse(VerticalTabUtils.isVerticalTabsEnabled(null));
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
                LayoutSwitchEntryPoint.APP_MENU,
                /* isEnabling= */ true,
                LayoutToggleSourceAndDirection.ENABLE_APP_MENU);
    }

    @Test
    @SmallTest
    public void testRecordLayoutToggle_Enable_TabContextMenu() {
        assertLayoutToggleHistogram(
                LayoutSwitchEntryPoint.TAB_CONTEXT_MENU,
                /* isEnabling= */ true,
                LayoutToggleSourceAndDirection.ENABLE_TAB_CONTEXT_MENU);
    }

    @Test
    @SmallTest
    public void testRecordLayoutToggle_Enable_TabStripContextMenu() {
        assertLayoutToggleHistogram(
                LayoutSwitchEntryPoint.TAB_STRIP_CONTEXT_MENU,
                /* isEnabling= */ true,
                LayoutToggleSourceAndDirection.ENABLE_TAB_STRIP_CONTEXT_MENU);
    }

    @Test
    @SmallTest
    public void testRecordLayoutToggle_Disable_AppMenu() {
        assertLayoutToggleHistogram(
                LayoutSwitchEntryPoint.APP_MENU,
                /* isEnabling= */ false,
                LayoutToggleSourceAndDirection.DISABLE_APP_MENU);
    }

    @Test
    @SmallTest
    public void testRecordLayoutToggle_Disable_TabContextMenu() {
        assertLayoutToggleHistogram(
                LayoutSwitchEntryPoint.TAB_CONTEXT_MENU,
                /* isEnabling= */ false,
                LayoutToggleSourceAndDirection.DISABLE_TAB_CONTEXT_MENU);
    }

    @Test
    @SmallTest
    public void testRecordLayoutToggle_Disable_TabStripContextMenu() {
        assertLayoutToggleHistogram(
                LayoutSwitchEntryPoint.TAB_STRIP_CONTEXT_MENU,
                /* isEnabling= */ false,
                LayoutToggleSourceAndDirection.DISABLE_TAB_STRIP_CONTEXT_MENU);
    }

    @Test
    @SmallTest
    public void testRecordLayoutToggle_NullResources_DoesNotCrash() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.VerticalTabs.LayoutToggleSourceAndDirection",
                                LayoutToggleSourceAndDirection.ENABLE_APP_MENU)
                        .expectNoRecords("Android.VerticalTabs.WindowWidthBoundaryOnToggle.Enable")
                        .build();

        VerticalTabUtils.recordLayoutToggle(
                mMockContext, LayoutSwitchEntryPoint.APP_MENU, /* isEnabling= */ true);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void testRecordLayoutToggle_NullContext() {
        assertThrows(
                AssertionError.class,
                () ->
                        VerticalTabUtils.recordLayoutToggle(
                                null, LayoutSwitchEntryPoint.APP_MENU, /* isEnabling= */ true));
    }

    private void assertLayoutToggleHistogram(
            @LayoutSwitchEntryPoint int entryPoint,
            boolean isEnabling,
            @LayoutToggleSourceAndDirection int expectedEnumVal) {
        String expectedHistogram =
                isEnabling
                        ? "Android.VerticalTabs.WindowWidthBoundaryOnToggle.Enable"
                        : "Android.VerticalTabs.WindowWidthBoundaryOnToggle.Disable";
        int widthDp = mContext.getResources().getConfiguration().screenWidthDp;
        @WindowWidthBoundary
        int expectedBoundary = VerticalTabUtils.getWindowWidthBoundary(widthDp);
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.VerticalTabs.LayoutToggleSourceAndDirection",
                                expectedEnumVal)
                        .expectIntRecord(expectedHistogram, expectedBoundary)
                        .build();

        VerticalTabUtils.recordLayoutToggle(mContext, entryPoint, isEnabling);

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

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testIsTablet_TrueOnTabletNonDesktop() {
        DeviceInfo.setIsDesktopForTesting(false);
        assertTrue(VerticalTabUtils.isTablet(mContext));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testIsTablet_FalseOnDesktop() {
        DeviceInfo.setIsDesktopForTesting(true);
        assertFalse(VerticalTabUtils.isTablet(mContext));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw400dp")
    public void testIsTablet_FalseOnPhone() {
        DeviceInfo.setIsDesktopForTesting(false);
        assertFalse(VerticalTabUtils.isTablet(mContext));
    }

    @Test
    @SmallTest
    public void testIsTablet_NullContext() {
        assertThrows(AssertionError.class, () -> VerticalTabUtils.isTablet(null));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testShouldShowNewBadgeForVerticalTabs_NullContext() {
        FeatureOverrides.enable(ChromeFeatureList.ANDROID_VERTICAL_TABS);
        DeviceInfo.setIsDesktopForTesting(false);
        assertFalse(VerticalTabUtils.shouldShowNewBadgeForVerticalTabs(null, null));
    }

    @Test
    @SmallTest
    public void testGetWindowWidthBoundary() {
        // < 488dp (412 + 76): NOT_SHOWABLE
        assertEquals(
                WindowWidthBoundary.NOT_SHOWABLE, VerticalTabUtils.getWindowWidthBoundary(487));

        // [488dp, 504dp): FORCED_COLLAPSED
        assertEquals(
                WindowWidthBoundary.FORCED_COLLAPSED, VerticalTabUtils.getWindowWidthBoundary(488));
        assertEquals(
                WindowWidthBoundary.FORCED_COLLAPSED, VerticalTabUtils.getWindowWidthBoundary(503));

        // [504dp, 726dp): DYNAMIC_EXPANDABLE
        assertEquals(
                WindowWidthBoundary.DYNAMIC_EXPANDABLE,
                VerticalTabUtils.getWindowWidthBoundary(504));
        assertEquals(
                WindowWidthBoundary.DYNAMIC_EXPANDABLE,
                VerticalTabUtils.getWindowWidthBoundary(725));

        // >= 726dp: FULLY_EXPANDABLE
        assertEquals(
                WindowWidthBoundary.FULLY_EXPANDABLE, VerticalTabUtils.getWindowWidthBoundary(726));
        assertEquals(
                WindowWidthBoundary.FULLY_EXPANDABLE,
                VerticalTabUtils.getWindowWidthBoundary(1000));
    }

    @Test
    @SmallTest
    public void testGetWindowWidthBoundary_CustomAvailableWidth() {
        // Available width < 76dp -> NOT_SHOWABLE regardless of window width
        assertEquals(
                WindowWidthBoundary.NOT_SHOWABLE,
                VerticalTabUtils.getWindowWidthBoundary(
                        /* windowWidthDp= */ 800, /* availableWidthDp= */ 75));

        // Available width between 76dp and 91dp in wide window -> FORCED_COLLAPSED
        assertEquals(
                WindowWidthBoundary.FORCED_COLLAPSED,
                VerticalTabUtils.getWindowWidthBoundary(
                        /* windowWidthDp= */ 800, /* availableWidthDp= */ 76));
        assertEquals(
                WindowWidthBoundary.FORCED_COLLAPSED,
                VerticalTabUtils.getWindowWidthBoundary(
                        /* windowWidthDp= */ 800, /* availableWidthDp= */ 91));

        // Available width >= 92dp restricts wide window to dynamic expandable
        assertEquals(
                WindowWidthBoundary.DYNAMIC_EXPANDABLE,
                VerticalTabUtils.getWindowWidthBoundary(
                        /* windowWidthDp= */ 800, /* availableWidthDp= */ 92));
        assertEquals(
                WindowWidthBoundary.DYNAMIC_EXPANDABLE,
                VerticalTabUtils.getWindowWidthBoundary(
                        /* windowWidthDp= */ 800, /* availableWidthDp= */ 150));

        // Available width >= 240dp and window is wide -> FULLY_EXPANDABLE
        assertEquals(
                WindowWidthBoundary.FULLY_EXPANDABLE,
                VerticalTabUtils.getWindowWidthBoundary(
                        /* windowWidthDp= */ 800, /* availableWidthDp= */ 300));
    }
}
