// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tasks.tab_management.TabSearchOverlayCoordinator.TabSearchEntryPoint;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.tab_search.TabSearchOverlayFacility;
import org.chromium.chrome.test.transit.vertical_tabs.VerticalTabsFacility;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests for the Tab Search Overlay opened from the vertical tabs rail. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({
    ChromeFeatureList.TAB_SEARCH_FOR_DESKTOP,
    ChromeFeatureList.ANDROID_VERTICAL_TABS + ":enable_by_default/true"
})
@Restriction({DeviceFormFactor.DESKTOP})
@Batch(Batch.PER_CLASS)
public class TabSearchOverlayVerticalTabsTest {
    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private WebPageStation mPage;

    @Before
    public void setUp() {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.VERTICAL_TABS_ENABLED, true);
        mPage = mCtaTestRule.startOnTestServerUrl("/chrome/test/data/android/navigate/one.html");
    }

    @Test
    @MediumTest
    public void testEntryPoint_VerticalTabs() {
        VerticalTabsFacility<CtaPageStation> verticalTabs = mPage.expectVerticalTabs();
        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.EntryPoint", TabSearchEntryPoint.VERTICAL_TABS);

        TabSearchOverlayFacility<CtaPageStation> tabSearchOverlay =
                verticalTabs.openTabSearchOverlay();
        watcher.assertExpected();

        tabSearchOverlay.dismissViaCloseButton();
    }
}
