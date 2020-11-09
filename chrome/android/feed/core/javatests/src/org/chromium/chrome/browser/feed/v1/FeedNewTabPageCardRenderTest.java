// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v1;

import static androidx.test.espresso.Espresso.onView;

import static org.hamcrest.Matchers.instanceOf;

import android.view.ViewGroup;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.IntegrationTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.snippets.SectionHeader;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.List;

/**
 * Tests for {@link NewTabPage} with card rendering. Other tests can be found in
 * {@link org.chromium.chrome.browser.feed.FeedNewTabPageTest}.
 */
// clang-format off
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "disable-features=IPH_FeedHeaderMenu"})
@Features.EnableFeatures({ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS,
    ChromeFeatureList.OMNIBOX_SEARCH_ENGINE_LOGO})
@Features.DisableFeatures({ChromeFeatureList.REPORT_FEED_USER_ACTIONS,
    ChromeFeatureList.QUERY_TILES, ChromeFeatureList.VIDEO_TUTORIALS,
    ChromeFeatureList.ENHANCED_PROTECTION_PROMO_CARD,
    ChromeFeatureList.INTEREST_FEED_V2, ChromeFeatureList.INTEREST_FEED_NOTICE_CARD_AUTO_DISMISS,
    ChromeFeatureList.INTEREST_FEEDV1_CLICKS_AND_VIEWS_CONDITIONAL_UPLOAD})
public class FeedNewTabPageCardRenderTest {
    // clang-format on
    private static final String TEST_FEED_DATA_BASE_PATH = "/chrome/test/data/android/feed/";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    @Rule
    public FeedDataInjectRule mFeedDataInjector = new FeedDataInjectRule(true);

    @Rule
    public EmbeddedTestServerRule mTestServer = new EmbeddedTestServerRule();

    private Tab mTab;
    private NewTabPage mNtp;
    private ViewGroup mTileGridLayout;
    private FakeMostVisitedSites mMostVisitedSites;
    private List<SiteSuggestion> mSiteSuggestions;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mSiteSuggestions = NewTabPageTestUtils.createFakeSiteSuggestions(mTestServer.getServer());
        mMostVisitedSites = new FakeMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(mSiteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mMostVisitedSites;

        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();
        mTileGridLayout = mNtp.getView().findViewById(R.id.tile_grid_layout);
        Assert.assertEquals(mSiteSuggestions.size(), mTileGridLayout.getChildCount());
    }

    @Test
    @MediumTest
    @Feature({"FeedNewTabPage", "RenderTest"})
    @DataFilePath(TEST_FEED_DATA_BASE_PATH + "feed_world.gcl.bin")
    @IntegrationTest
    // The IntegrationTest annotation skips this test on android-arm-official-tests.
    public void testFeedCardRenderingScenarioWorld() throws Exception {
        renderFeedCards("world");
    }

    private void renderFeedCards(String scenarioName) throws Exception {
        // Open a new tab.
        SectionHeader firstHeader = mNtp.getCoordinatorForTesting()
                                            .getMediatorForTesting()
                                            .getSectionHeaderForTesting();
        RecyclerView recycleView =
                (RecyclerView) mNtp.getCoordinatorForTesting().getStreamForTesting().getView();

        // Check header is expanded.
        Assert.assertTrue(firstHeader.isExpandable() && firstHeader.isExpanded());
        Assert.assertTrue(getPreferenceForArticleSectionHeader());

        // Trigger a refresh to get feed cards.
        mFeedDataInjector.triggerFeedRefreshOnUiThreadBlocking(
                mNtp.getCoordinatorForTesting().getStreamForTesting());

        // Scroll to the first feed card.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(
                        mFeedDataInjector.getFirstCardPosition()));

        RecyclerViewTestUtils.waitForStableRecyclerView(recycleView);
        mRenderTestRule.render(
                recycleView, String.format("render_feed_cards_top_%s", scenarioName));

        // Scroll to the bottom.
        RecyclerViewTestUtils.scrollToBottom(recycleView);
        RecyclerViewTestUtils.waitForStableRecyclerView(recycleView);
        mRenderTestRule.render(
                recycleView, String.format("render_feed_cards_bottom_%s", scenarioName));

        // Scroll to the first feed card again.
        onView(instanceOf(RecyclerView.class))
                .perform(RecyclerViewActions.scrollToPosition(
                        mFeedDataInjector.getFirstCardPosition()));
        RecyclerViewTestUtils.waitForStableRecyclerView(recycleView);
        mRenderTestRule.render(
                recycleView, String.format("render_feed_cards_top_again_%s", scenarioName));
    }

    private boolean getPreferenceForArticleSectionHeader() throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> UserPrefs.get(Profile.getLastUsedRegularProfile())
                                   .getBoolean(Pref.ARTICLES_LIST_VISIBLE));
    }
}
