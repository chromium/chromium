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

import static org.chromium.chrome.browser.query_tiles.ListMatchers.isScrolledToFront;
import static org.chromium.chrome.browser.query_tiles.ListMatchers.matchList;
import static org.chromium.chrome.browser.query_tiles.ViewActions.scrollTo;

import android.support.test.InstrumentationRegistry;
import android.view.KeyEvent;
import android.view.View;

import androidx.test.espresso.assertion.ViewAssertions;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.components.query_tiles.TestTileProvider;

/**
 * Provides a set of tests to validate that QueryTiles works properly in the NTP given the following
 * experiment flags:
 * - ChromeFeatureList.QUERY_TILES
 * - ChromeFeatureList.QUERY_TILES_ENABLE_QUERY_EDITING
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.QUERY_TILES, ChromeFeatureList.QUERY_TILES_IN_NTP,
        ChromeFeatureList.QUERY_TILES_ENABLE_QUERY_EDITING})
public class QueryTileSectionToOmniboxTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private Tab mTab;
    private TestTileProvider mTileProvider;

    private void setUp(int levels) {
        mTileProvider = new TestTileProvider(levels, 8 /* count */);
        TileProviderFactory.setTileProviderForTesting(mTileProvider);
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);
    }

    /** Tests that tiles show up as expected whe the NTP is opened. */
    @Test
    @SmallTest
    public void testShowQueryTileSection() throws Exception {
        setUp(1 /* levels */);
        matchList(withParent(withId(R.id.query_tiles)), mTileProvider.getChildrenOf());
    }

    /** Tests that clicking a tile on a single depth list opens the omnibox. */
    @Test
    @SmallTest
    public void testClickTileOpensOmnibox() throws Exception {
        setUp(1 /* levels */);
        Matcher<View> recyclerViewMatcher = withParent(withId(R.id.query_tiles));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf());

        // Simulate a click on the tile.
        QueryTile tile = mTileProvider.getTileAt(0);
        onView(recyclerViewMatcher).perform(scrollTo(0));
        onView(withText(tile.displayTitle)).perform(click());

        // Make sure the omnibox opens.
        OmniboxTestUtils.waitForFocusAndKeyboardActive(
                (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar), true);
        onView(withId(R.id.url_bar))
                .check(ViewAssertions.matches(
                        withText(org.hamcrest.Matchers.containsString(tile.queryText))));
    }

    /** Tests that clicking a tile on a multi-depth list opens the omnibox. */
    @Test
    @SmallTest
    public void testClickTileOpensOmniboxTwoTiers() throws Exception {
        setUp(2 /* levels */);
        Matcher<View> recyclerViewMatcher = withParent(withId(R.id.query_tiles));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf());

        // Simulate a click on the first tile.
        QueryTile tile1 = mTileProvider.getTileAt(3);
        QueryTile tile2 = mTileProvider.getTileAt(3, 2);
        onView(recyclerViewMatcher).perform(scrollTo(3));
        onView(withText(tile1.displayTitle)).perform(click());
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf(3));

        // Simulate a click on the second tile.
        onView(recyclerViewMatcher).perform(scrollTo(2));
        onView(withText(tile2.displayTitle)).perform(click());

        // Make sure the omnibox opens.
        OmniboxTestUtils.waitForFocusAndKeyboardActive(
                (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar), true);
        onView(withId(R.id.url_bar))
                .check(ViewAssertions.matches(
                        withText(org.hamcrest.Matchers.containsString(tile2.queryText))));
    }

    /** Test that clicking on a tile to open the omnibox and pressing back shows the right tiles. */
    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1091225")
    public void testBackOutOfOmniboxRestoresTilePosition() throws Exception {
        setUp(1 /* levels */);
        Matcher<View> recyclerViewMatcher = withParent(withId(R.id.query_tiles));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf());

        // Simulate a click on the tile.
        QueryTile tile = mTileProvider.getTileAt(0);
        onView(recyclerViewMatcher).perform(scrollTo(0));
        onView(withText(tile.displayTitle)).perform(click());

        // Make sure the omnibox opens.
        UrlBar urlBar = mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);
        onView(withId(R.id.url_bar))
                .check(ViewAssertions.matches(
                        withText(org.hamcrest.Matchers.containsString(tile.queryText))));

        // Press back to hide the omnibox.
        // Press back twice - once to minimize keyboard, once to minimize omnibox (sigh).
        InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        InstrumentationRegistry.getInstrumentation().sendKeyDownUpSync(KeyEvent.KEYCODE_BACK);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, false);
        onView(recyclerViewMatcher).check(ViewAssertions.matches(isScrolledToFront()));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf());
    }

    @Test
    @SmallTest
    public void testFocusOmniboxWithZeroSuggest() {
        setUp(1 /* levels */);
        Matcher<View> recyclerViewMatcher = withParent(withId(R.id.query_tiles));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf());
        onView(withId(R.id.query_tiles_chip)).check(matches(not(isDisplayed())));

        // Click on the search box. Omnibox should show up.
        onView(withId(R.id.search_box_text)).perform(click());
        UrlBar urlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        OmniboxTestUtils.waitForFocusAndKeyboardActive(urlBar, true);
        Assert.assertTrue(urlBar.getText().toString().isEmpty());
    }
}