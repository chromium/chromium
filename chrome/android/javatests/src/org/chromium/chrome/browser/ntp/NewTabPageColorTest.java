// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.res.Resources;
import android.graphics.Color;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.feed.v1.DataFilePath;
import org.chromium.chrome.browser.feed.v1.FeedDataInjectRule;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.test.util.UiRestriction;

/**
 * Tests for colors used in UI components in the native android New Tab Page.
 */
// clang-format off
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
                       "disable-features=IPH_FeedHeaderMenu"})
@Features.DisableFeatures({ChromeFeatureList.EXPLORE_SITES,
                           ChromeFeatureList.REPORT_FEED_USER_ACTIONS,
                           ChromeFeatureList.QUERY_TILES, ChromeFeatureList.VIDEO_TUTORIALS,
                           ChromeFeatureList.INTEREST_FEED_NOTICE_CARD_AUTO_DISMISS})
// clang-format on
public class NewTabPageColorTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Rule
    public FeedDataInjectRule mFeedDataInjector = new FeedDataInjectRule(true);

    private static final String TEST_FEED_DATA_BASE_PATH = "/chrome/test/data/android/feed/";

    private Tab mTab;
    private NewTabPage mNtp;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityWithURL("about:blank");

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();
    }

    // clang-format off
    @Test
    @MediumTest
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    @Feature({"NewTabPage", "FeedNewTabPage"})
    @Features.EnableFeatures({ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS,
                              ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO})
    @Features.DisableFeatures({
            ChromeFeatureList.ENHANCED_PROTECTION_PROMO_CARD,
            ChromeFeatureList.INTEREST_FEED_V2,
            ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD})
    @DataFilePath(TEST_FEED_DATA_BASE_PATH + "feed_world.gcl.bin")
    public void testTextBoxBackgroundColor() throws Exception {
        // clang-format on
        RecyclerView recycleView =
                (RecyclerView) mNtp.getCoordinatorForTesting().getStreamForTesting().getView();

        Resources resources = mActivityTestRule.getActivity().getResources();
        Assert.assertEquals(ChromeColors.getPrimaryBackgroundColor(resources, false),
                mNtp.getToolbarTextBoxBackgroundColor(Color.BLACK));

        // Trigger a refresh to get feed cards so that page becomes long enough
        // for the location bar to get placed to the top of the page after scrolling.
        mFeedDataInjector.triggerFeedRefreshOnUiThreadBlocking(
                mNtp.getCoordinatorForTesting().getStreamForTesting());

        // Scroll to the bottom.
        RecyclerViewTestUtils.scrollToBottom(recycleView);
        RecyclerViewTestUtils.waitForStableRecyclerView(recycleView);

        Assert.assertTrue(mNtp.isLocationBarScrolledToTopInNtp());
        Assert.assertEquals(
                ApiCompatibilityUtils.getColor(resources, R.color.toolbar_text_box_background),
                mNtp.getToolbarTextBoxBackgroundColor(Color.BLACK));
    }
}
