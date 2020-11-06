// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.query_tiles.ListMatchers.matchList;
import static org.chromium.chrome.browser.query_tiles.TileMatchers.withChip;
import static org.chromium.chrome.browser.query_tiles.ViewActions.scrollTo;

import android.view.View;
import android.widget.TextView;

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
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.components.query_tiles.TestTileProvider;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for the query tiles section on new tab page.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.QUERY_TILES, ChromeFeatureList.QUERY_TILES_IN_NTP})
public class QueryTileSectionTest {
    private static final String SEARCH_URL_PATTERN = "https://www.google.com/search?q=";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private Tab mTab;
    private TestTileProvider mTileProvider;

    @Before
    public void setUp() {
        mTileProvider = new TestTileProvider(2 /* levels */, 8 /* count */);
        TileProviderFactory.setTileProviderForTesting(mTileProvider);
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);
    }

    @After
    public void tearDown() {}

    @Test
    @SmallTest
    public void testShowQueryTileSection() throws Exception {
        matchList(withParent(withId(R.id.query_tiles)), mTileProvider.getChildrenOf());
        onView(withId(R.id.query_tiles_chip)).check(matches(not(isDisplayed())));
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
    public void testSearchWithFirstLevelTile() throws Exception {
        // Click first level tile. Chip should show up.
        QueryTile tile = mTileProvider.getTileAt(0);
        onView(withText(tile.displayTitle)).perform(click());
        matchList(withParent(withId(R.id.query_tiles)), mTileProvider.getChildrenOf(0));

        // Click on the chip. SRP should show up with first level query text.
        onView(withId(R.id.query_tiles_chip)).check(matches(isDisplayed()));
        onView(withId(R.id.query_tiles_chip)).perform(click());
        waitForSearchResultsPage();
        Assert.assertTrue(getTabUrl().contains(tile.queryText));
    }

    @Test
    @SmallTest
    public void testChipVisibilityOnFakeBox() throws Exception {
        Matcher<View> recyclerViewMatcher = withParent(withId(R.id.query_tiles));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf());

        // No chip should be shown by default.
        onView(withId(R.id.query_tiles_chip)).check(matches(not(isDisplayed())));

        // Chip shows up when first level tile clicked.
        QueryTile tile = mTileProvider.getTileAt(0);
        onView(recyclerViewMatcher).perform(scrollTo(0));
        onView(withText(tile.displayTitle)).perform(click());
        onView(withId(R.id.query_tiles_chip)).check(matches(withChip(tile)));

        // Chip disappears on hitting clear button.
        onView(withId(R.id.chip_cancel_btn)).perform(click());
        onView(withId(R.id.query_tiles_chip)).check(matches(not(isDisplayed())));
    }

    @Test
    @SmallTest
    public void testClearingSelectedTileBringsBackTopLevelTiles() throws Exception {
        Matcher<View> recyclerViewMatcher = withParent(withId(R.id.query_tiles));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf());

        // Navigate to second level tile.
        QueryTile tile = mTileProvider.getTileAt(0);
        QueryTile subtile = mTileProvider.getTileAt(0, 0);
        onView(recyclerViewMatcher).perform(scrollTo(0));
        onView(withText(tile.displayTitle)).perform(click());
        onView(withId(R.id.query_tiles_chip)).check(matches(withChip(tile)));
        onView(withText(subtile.displayTitle)).check(matches(isDisplayed()));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf(0));

        // Clear selected chip. We should be back at top level tiles.
        onView(withId(R.id.chip_cancel_btn)).perform(click());
        onView(withId(R.id.query_tiles_chip)).check(matches(not(isDisplayed())));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf());
    }

    @Test
    @SmallTest
    public void testFocusOmniboxWithZeroSuggest() throws Exception {
        Matcher<View> recyclerViewMatcher = withParent(withId(R.id.query_tiles));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf());
        onView(withId(R.id.query_tiles_chip)).check(matches(not(isDisplayed())));

        // Click on the search box. Omnibox should show up.
        onView(withId(R.id.search_box_text)).perform(click());
        UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);
        Assert.assertTrue(urlBar.getText().toString().isEmpty());
    }

    @Test
    @SmallTest
    public void testFocusOmniboxWithFirstLevelQueryText() throws Exception {
        Matcher<View> recyclerViewMatcher = withParent(withId(R.id.query_tiles));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf());

        // No chip should be shown by default.
        onView(withId(R.id.query_tiles_chip)).check(matches(not(isDisplayed())));

        // Chip shows up when first level tile clicked.
        QueryTile tile = mTileProvider.getTileAt(0);
        onView(recyclerViewMatcher).perform(scrollTo(0));
        onView(withText(tile.displayTitle)).perform(click());
        onView(withId(R.id.query_tiles_chip)).check(matches(withChip(tile)));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf(0));

        // Click on the search box. Omnibox should show up with first level query text.
        clickSearchBox();
        UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);
        Assert.assertTrue(urlBar.getText().toString().contains(tile.queryText));
    }

    private void clickSearchBox() {
        // Directly clicking search box using espresso doesn't work and seems to forward the click
        // event to the chip cancel button instead.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TextView searchBox =
                    (TextView) mActivityTestRule.getActivity().findViewById(R.id.search_box_text);
            searchBox.performClick();
        });
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
