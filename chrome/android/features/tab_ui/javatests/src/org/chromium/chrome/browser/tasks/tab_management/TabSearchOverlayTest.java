// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabSearchOverlayCoordinator.TabSearchDismissalReason;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.tab_search.TabSearchOverlayFacility.SuggestionFacility;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests for the Tab Search Overlay using Public Transit. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.TAB_SEARCH_FOR_DESKTOP})
@Restriction({DeviceFormFactor.DESKTOP})
@Batch(Batch.PER_CLASS)
public class TabSearchOverlayTest {
    private static final int SERVER_PORT = 13245;

    @Rule
    public AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private EmbeddedTestServer mTestServer;
    private WebPageStation mPage;

    @Before
    public void setUp() {
        mTestServer =
                mCtaTestRule.getEmbeddedTestServerRule().setServerPort(SERVER_PORT).getServer();
    }

    @Test
    @MediumTest
    public void testOpenExistingTab_ReusesTabWithoutIncreasingCount() {
        String url1 = mTestServer.getURL("/chrome/test/data/android/navigate/one.html");
        String url2 = mTestServer.getURL("/chrome/test/data/android/navigate/two.html");

        mPage = mCtaTestRule.startOnWebPage(url1);
        Tab tab1 = mPage.getTab();

        mPage = mPage.openFakeLinkToWebPage(url2);
        Tab tab2 = mPage.getTab();

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.TabSearch.DismissalReason", TabSearchDismissalReason.TAB_SELECTED);

        var tabSearchOverlay = mPage.openTabSearchOverlay();
        tabSearchOverlay.typeInSearchBox("one.html");

        SuggestionFacility suggestion =
                tabSearchOverlay.findSuggestion(
                        /* index= */ null, /* title= */ "One", /* text= */ null);
        mPage = suggestion.clickToSelectTab();

        // Verify the existing tab was reused and tab count remained 2.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeTabbedActivity cta = mCtaTestRule.getActivity();
                    TabModel normalTabModel =
                            cta.getTabModelSelector().getModel(/* incognito= */ false);
                    assertEquals(2, normalTabModel.getCount());
                    assertEquals(tab1, mPage.getTab());
                    assertEquals("One", mPage.getTab().getTitle());

                    // Verify the overlay was dismissed with TAB_SELECTED.
                    TabbedRootUiCoordinator rootUiCoordinator =
                            (TabbedRootUiCoordinator) cta.getRootUiCoordinatorForTesting();
                    TabSearchOverlayCoordinator tabSearchCoordinator =
                            rootUiCoordinator.getTabSearchOverlayCoordinatorForTesting();
                    assertFalse(tabSearchCoordinator.isVisible());
                });
        watcher.assertExpected();
    }
}
