// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appearance.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.appearance.settings.TabPositionSettingsFragment.PREF_EXPAND_TABS_ON_HOVER_SWITCH;
import static org.chromium.chrome.browser.appearance.settings.TabPositionSettingsFragment.PREF_TAB_POSITION_CARD_SELECTOR;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils.ExpandOnHoverToggleEntryPoint;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;

/** Tests for {@link TabPositionSettingsFragment}. */
@Batch(Batch.PER_CLASS)
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.ANDROID_VERTICAL_TABS})
public class TabPositionSettingsFragmentTest {
    private static final int WIDE_WINDOW_WIDTH_DP = 800;
    private static final int NARROW_WINDOW_WIDTH_DP = 400;

    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsTestRule =
            new BlankUiTestActivitySettingsTestRule();

    private TabPositionSettingsFragment mSettings;
    private int mOriginalScreenWidthDp;

    @Before
    public void setUp() {
        VerticalTabUtils.setIsVerticalTabsEligibleForTesting(true);
        VerticalTabUtils.setTabLayoutSwitchingInProgress(false);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ChromeSharedPreferences.getInstance()
                                .writeBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false));
    }

    @After
    public void tearDown() {
        VerticalTabUtils.setTabLayoutSwitchingInProgress(false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (mSettingsTestRule.getActivity() != null && mOriginalScreenWidthDp > 0) {
                        mSettingsTestRule
                                        .getActivity()
                                        .getResources()
                                        .getConfiguration()
                                        .screenWidthDp =
                                mOriginalScreenWidthDp;
                    }
                    ChromeSharedPreferences.getInstance()
                            .removeKey(ChromePreferenceKeys.VERTICAL_TABS_ENABLED);
                    ChromeSharedPreferences.getInstance()
                            .removeKey(ChromePreferenceKeys.VERTICAL_TABS_EXPAND_ON_HOVER);
                });
    }

    private void launchSettings() {
        mSettingsTestRule.launchPreference(
                TabPositionSettingsFragment.class,
                null,
                fragment -> mSettings = (TabPositionSettingsFragment) fragment);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mOriginalScreenWidthDp =
                            mSettingsTestRule
                                    .getActivity()
                                    .getResources()
                                    .getConfiguration()
                                    .screenWidthDp;
                    mSettingsTestRule
                                    .getActivity()
                                    .getResources()
                                    .getConfiguration()
                                    .screenWidthDp =
                            WIDE_WINDOW_WIDTH_DP;
                });
    }

    @Test
    @SmallTest
    public void testTabPositionPreferenceIsPresent() {
        launchSettings();
        CriteriaHelper.pollUiThread(() -> mSettings.getCardPreferenceForTesting() != null);
        TabPositionCardPreference cardPref = mSettings.getCardPreferenceForTesting();
        assertNotNull(cardPref);
        assertEquals(PREF_TAB_POSITION_CARD_SELECTOR, cardPref.getKey());
    }

    @Test
    @SmallTest
    public void testToggleTabPositionUpdatesPreference() {
        launchSettings();

        CriteriaHelper.pollUiThread(() -> mSettings.getCardPreferenceForTesting() != null);
        TabPositionCardPreference cardPref = mSettings.getCardPreferenceForTesting();
        assertNotNull(cardPref);

        CriteriaHelper.pollUiThread(() -> cardPref.getHorizontalOptionForTesting() != null);
        CriteriaHelper.pollUiThread(() -> cardPref.getVerticalOptionForTesting() != null);

        View horizontalOption = cardPref.getHorizontalOptionForTesting();
        View verticalOption = cardPref.getVerticalOptionForTesting();

        // Initially horizontal should be selected (isVertical == false).
        assertFalse(cardPref.isVerticalTabsSelected());
        assertTrue(horizontalOption.isSelected());
        assertFalse(verticalOption.isSelected());

        // Select Vertical.
        ThreadUtils.runOnUiThreadBlocking(verticalOption::performClick);
        assertTrue(cardPref.isVerticalTabsSelected());
        assertFalse(horizontalOption.isSelected());
        assertTrue(verticalOption.isSelected());
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false));

        // Select Horizontal.
        ThreadUtils.runOnUiThreadBlocking(horizontalOption::performClick);
        assertFalse(cardPref.isVerticalTabsSelected());
        assertTrue(horizontalOption.isSelected());
        assertFalse(verticalOption.isSelected());
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false));
    }

    @Test
    @SmallTest
    public void testToggleTabPositionBlockedWhileSwitchingInProgress() {
        launchSettings();

        CriteriaHelper.pollUiThread(() -> mSettings.getCardPreferenceForTesting() != null);
        TabPositionCardPreference cardPref = mSettings.getCardPreferenceForTesting();
        assertNotNull(cardPref);

        CriteriaHelper.pollUiThread(() -> cardPref.getHorizontalOptionForTesting() != null);
        CriteriaHelper.pollUiThread(() -> cardPref.getVerticalOptionForTesting() != null);

        View horizontalOption = cardPref.getHorizontalOptionForTesting();
        View verticalOption = cardPref.getVerticalOptionForTesting();

        // Simulate an in-progress tab layout transition.
        VerticalTabUtils.setTabLayoutSwitchingInProgress(true);

        // Clicking Vertical should be ignored while switching is in progress.
        ThreadUtils.runOnUiThreadBlocking(verticalOption::performClick);
        assertFalse(cardPref.isVerticalTabsSelected());
        assertTrue(horizontalOption.isSelected());
        assertFalse(verticalOption.isSelected());
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false));

        // Once switching finishes, clicking Vertical should succeed.
        VerticalTabUtils.setTabLayoutSwitchingInProgress(false);
        ThreadUtils.runOnUiThreadBlocking(verticalOption::performClick);
        assertTrue(cardPref.isVerticalTabsSelected());
        assertFalse(horizontalOption.isSelected());
        assertTrue(verticalOption.isSelected());
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false));

        // Clicking Horizontal while switching is in progress should also be ignored.
        VerticalTabUtils.setTabLayoutSwitchingInProgress(true);
        ThreadUtils.runOnUiThreadBlocking(horizontalOption::performClick);
        assertTrue(cardPref.isVerticalTabsSelected());
        assertFalse(horizontalOption.isSelected());
        assertTrue(verticalOption.isSelected());
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false));

        // Once switching finishes, clicking Horizontal should succeed.
        VerticalTabUtils.setTabLayoutSwitchingInProgress(false);
        ThreadUtils.runOnUiThreadBlocking(horizontalOption::performClick);
        assertFalse(cardPref.isVerticalTabsSelected());
        assertTrue(horizontalOption.isSelected());
        assertFalse(verticalOption.isSelected());
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false));
    }

    @Test
    @SmallTest
    public void testToggleTabPositionBlockedWhenWindowTooNarrow() {
        launchSettings();

        CriteriaHelper.pollUiThread(() -> mSettings.getCardPreferenceForTesting() != null);
        TabPositionCardPreference cardPref = mSettings.getCardPreferenceForTesting();
        assertNotNull(cardPref);

        CriteriaHelper.pollUiThread(() -> cardPref.getHorizontalOptionForTesting() != null);
        CriteriaHelper.pollUiThread(() -> cardPref.getVerticalOptionForTesting() != null);

        View horizontalOption = cardPref.getHorizontalOptionForTesting();
        View verticalOption = cardPref.getVerticalOptionForTesting();

        // Simulate a narrow window width where Vertical Tabs is not showable / auto-hidden.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mSettings.getResources().getConfiguration().screenWidthDp =
                                NARROW_WINDOW_WIDTH_DP);

        // Clicking Vertical when Horizontal is selected should be ignored on a narrow window.
        ThreadUtils.runOnUiThreadBlocking(verticalOption::performClick);
        assertFalse(cardPref.isVerticalTabsSelected());
        assertTrue(horizontalOption.isSelected());
        assertFalse(verticalOption.isSelected());
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false));

        // Simulate Vertical Tabs already enabled and in the auto-hide state on a narrow window.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ChromeSharedPreferences.getInstance()
                                .writeBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, true));
        CriteriaHelper.pollUiThread(cardPref::isVerticalTabsSelected);

        // Clicking Horizontal while Vertical Tabs is in auto-hide state should also be ignored.
        ThreadUtils.runOnUiThreadBlocking(horizontalOption::performClick);
        assertTrue(cardPref.isVerticalTabsSelected());
        assertFalse(horizontalOption.isSelected());
        assertTrue(verticalOption.isSelected());
        assertTrue(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false));

        // Once the window is wide enough, clicking Horizontal should succeed.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mSettings.getResources().getConfiguration().screenWidthDp =
                                WIDE_WINDOW_WIDTH_DP);
        ThreadUtils.runOnUiThreadBlocking(horizontalOption::performClick);
        assertFalse(cardPref.isVerticalTabsSelected());
        assertTrue(horizontalOption.isSelected());
        assertFalse(verticalOption.isSelected());
        assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false));
    }

    @Test
    @SmallTest
    public void testExternalPreferenceChangeUpdatesCardSelection() {
        launchSettings();

        CriteriaHelper.pollUiThread(() -> mSettings.getCardPreferenceForTesting() != null);
        TabPositionCardPreference cardPref = mSettings.getCardPreferenceForTesting();
        assertNotNull(cardPref);
        assertFalse(cardPref.isVerticalTabsSelected());

        // External update to true.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ChromeSharedPreferences.getInstance()
                                .writeBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, true));
        CriteriaHelper.pollUiThread(cardPref::isVerticalTabsSelected);

        // External update to false.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ChromeSharedPreferences.getInstance()
                                .writeBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false));
        CriteriaHelper.pollUiThread(() -> !cardPref.isVerticalTabsSelected());
    }

    @Test
    @SmallTest
    public void testExpandOnHoverSwitch_VisibleOnlyWhenVertical() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS, "expand_on_hover", true);
        launchSettings();

        CriteriaHelper.pollUiThread(() -> mSettings.getExpandOnHoverSwitchForTesting() != null);
        ChromeSwitchPreference expandOnHoverSwitch = mSettings.getExpandOnHoverSwitchForTesting();
        assertEquals(PREF_EXPAND_TABS_ON_HOVER_SWITCH, expandOnHoverSwitch.getKey());

        // Hidden while horizontal is selected.
        assertFalse(expandOnHoverSwitch.isVisible());

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ChromeSharedPreferences.getInstance()
                                .writeBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, true));
        CriteriaHelper.pollUiThread(expandOnHoverSwitch::isVisible);
        // On by default.
        assertTrue(expandOnHoverSwitch.isChecked());

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ChromeSharedPreferences.getInstance()
                                .writeBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false));
        CriteriaHelper.pollUiThread(() -> !expandOnHoverSwitch.isVisible());
    }

    @Test
    @SmallTest
    public void testExpandOnHoverSwitch_HiddenWhenFeatureDisabled() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS, "expand_on_hover", false);
        ThreadUtils.runOnUiThreadBlocking(() -> VerticalTabUtils.setVerticalTabsEnabled(true));
        launchSettings();

        CriteriaHelper.pollUiThread(() -> mSettings.getExpandOnHoverSwitchForTesting() != null);
        assertFalse(mSettings.getExpandOnHoverSwitchForTesting().isVisible());
    }

    @Test
    @SmallTest
    public void testExpandOnHoverSwitch_UpdatesAndFollowsPreference() {
        FeatureOverrides.overrideParam(
                ChromeFeatureList.ANDROID_VERTICAL_TABS, "expand_on_hover", true);
        ThreadUtils.runOnUiThreadBlocking(() -> VerticalTabUtils.setVerticalTabsEnabled(true));
        launchSettings();

        CriteriaHelper.pollUiThread(() -> mSettings.getExpandOnHoverSwitchForTesting() != null);
        ChromeSwitchPreference expandOnHoverSwitch = mSettings.getExpandOnHoverSwitchForTesting();
        CriteriaHelper.pollUiThread(expandOnHoverSwitch::isVisible);
        assertTrue(expandOnHoverSwitch.isChecked());

        // Turning the switch off updates the user setting.
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.VerticalTabs.ExpandOnHoverToggle.Disable",
                        ExpandOnHoverToggleEntryPoint.SETTINGS);
        ThreadUtils.runOnUiThreadBlocking(expandOnHoverSwitch::performClick);
        assertFalse(expandOnHoverSwitch.isChecked());
        assertFalse(VerticalTabUtils.isExpandOnHoverEnabled());
        histogramWatcher.assertExpected();

        // Changing the user setting elsewhere (e.g. a context menu) updates the switch.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        VerticalTabUtils.setExpandOnHoverEnabled(
                                true, ExpandOnHoverToggleEntryPoint.TAB_STRIP_CONTEXT_MENU));
        CriteriaHelper.pollUiThread(expandOnHoverSwitch::isChecked);
    }

    @Test
    @SmallTest
    public void testCardContainerResponsiveOrientation() {
        launchSettings();
        Context context = mSettingsTestRule.getActivity();
        int breakpointPx =
                context.getResources()
                        .getDimensionPixelSize(
                                R.dimen.tab_position_card_container_width_breakpoint);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabPositionCardContainer container =
                            (TabPositionCardContainer)
                                    LayoutInflater.from(context)
                                            .inflate(R.layout.tab_position_card_preference, null);

                    // Measure with width above 480dp breakpoint -> horizontal.
                    int wideWidthSpec =
                            View.MeasureSpec.makeMeasureSpec(
                                    breakpointPx + 100, View.MeasureSpec.EXACTLY);
                    int heightSpec =
                            View.MeasureSpec.makeMeasureSpec(800, View.MeasureSpec.AT_MOST);
                    container.measure(wideWidthSpec, heightSpec);
                    assertEquals(LinearLayout.HORIZONTAL, container.getOrientation());

                    // Measure with unspecified width should not flip orientation to vertical.
                    int unspecifiedWidthSpec =
                            View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
                    container.measure(unspecifiedWidthSpec, heightSpec);
                    assertEquals(LinearLayout.HORIZONTAL, container.getOrientation());

                    // Measure with width below 480dp breakpoint -> vertical.
                    int narrowWidthSpec =
                            View.MeasureSpec.makeMeasureSpec(
                                    breakpointPx - 100, View.MeasureSpec.EXACTLY);
                    container.measure(narrowWidthSpec, heightSpec);
                    assertEquals(LinearLayout.VERTICAL, container.getOrientation());
                });
    }
}
