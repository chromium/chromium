// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.isEmptyString;

import static org.chromium.chrome.browser.query_tiles.ListMatchers.matchList;
import static org.chromium.chrome.browser.query_tiles.ViewActions.scrollTo;

import android.view.View;

import androidx.test.espresso.IdlingPolicies;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DumpThreadsOnFailureRule;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.components.query_tiles.TestTileProvider;

import java.util.concurrent.TimeUnit;

/**
 * Tests for the query tiles section on new tab page.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.QUERY_TILES, ChromeFeatureList.QUERY_TILES_IN_NTP})
@DisableFeatures({ChromeFeatureList.QUERY_TILES_SEGMENTATION})
public class QueryTileSectionTest {
    private static final String SEARCH_URL_PATTERN = "https://www.google.com/search?q=";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    // TODO(https://crbug.com/1469931) Remove after flakes are understood/fixed.
    @Rule
    public DumpThreadsOnFailureRule mDumpThreadsOnFailureRule = new DumpThreadsOnFailureRule();

    private Tab mTab;
    private TestTileProvider mTileProvider;
    private OmniboxTestUtils mOmnibox;

    @Before
    public void setUp() {
        // Espresso defaults to 60 seconds, but this is often too long, as our tests cases are
        // killed in a similar amount of time. Shorten this to make it more likely that we'll fail
        // in Java, allowing our mDumpThreadsOnFailureRule to trigger.
        IdlingPolicies.setMasterPolicyTimeout(10, TimeUnit.SECONDS);

        mTileProvider = new TestTileProvider(2 /* levels */, 8 /* count */);
        TileProviderFactory.setTileProviderForTesting(mTileProvider);
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);
        mOmnibox = new OmniboxTestUtils(mActivityTestRule.getActivity());
    }

    @After
    public void tearDown() {}

    @Test
    @SmallTest
    public void testShowQueryTileSection() throws Exception {
        matchList(withParent(withId(R.id.query_tiles)), mTileProvider.getChildrenOf());
    }

    @Test
    @SmallTest
    public void testSearchWithLastLevelTile() throws Exception {
        Matcher<View> recyclerViewMatcher = withParent(withId(R.id.query_tiles));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf());

        // Click on first level tile.
        QueryTile tile = mTileProvider.getTileAt(0);
        onView(recyclerViewMatcher).perform(scrollTo(0));
        onView(withText(tile.displayTitle)).perform(click());
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf(0));

        // Click on last level tile. We should navigate to SRP with the query text.
        onView(recyclerViewMatcher).perform(scrollTo(0));
        QueryTile subtile = mTileProvider.getTileAt(0, 0);
        onView(withText(subtile.displayTitle)).perform(click());
        waitForSearchResultsPage();
        Assert.assertTrue(getTabUrl().contains(subtile.queryText));
    }

    @Test
    @SmallTest
    public void testFocusOmniboxWithZeroSuggest() throws Exception {
        Matcher<View> recyclerViewMatcher = withParent(withId(R.id.query_tiles));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf());

        // Click on the search box. Omnibox should show up.
        onView(withId(R.id.search_box_text)).perform(click());
        mOmnibox.checkFocus(true);
        mOmnibox.checkText(isEmptyString(), null);
    }

    private String getTabUrl() {
        return ChromeTabUtils.getUrlOnUiThread(mActivityTestRule.getActivity().getActivityTab())
                .getValidSpecOrEmpty();
    }

    private void waitForSearchResultsPage() {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("The SRP was never loaded.",
                    ChromeTabUtils.getUrlOnUiThread(mTab).getValidSpecOrEmpty(),
                    Matchers.containsString(SEARCH_URL_PATTERN));
        });
    }
}
