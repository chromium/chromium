// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appearance.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.settings.BlankUiTestActivitySettingsTestRule;

/** Tests for {@link TabPositionSettingsFragment}. */
@Batch(Batch.PER_CLASS)
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.ANDROID_VERTICAL_TABS})
public class TabPositionSettingsFragmentTest {
    @Rule
    public final BlankUiTestActivitySettingsTestRule mSettingsTestRule =
            new BlankUiTestActivitySettingsTestRule();

    private TabPositionSettingsFragment mSettings;

    @Before
    public void setUp() {
        VerticalTabUtils.setIsVerticalTabsEligibleForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, false);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.VERTICAL_TABS_ENABLED);
    }

    private void launchSettings() {
        mSettingsTestRule.launchPreference(
                TabPositionSettingsFragment.class,
                null,
                fragment -> mSettings = (TabPositionSettingsFragment) fragment);
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
