// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.query_tiles;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.clearText;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.chromium.chrome.browser.query_tiles.ListMatchers.matchList;
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

import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.profiles.Profile;
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
import org.chromium.components.query_tiles.TileProvider;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for the query tiles section on the omnibox. The test uses a fake server in native to
 * perform a full end-to-end test for omnibox query tiles.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Features.EnableFeatures({ChromeFeatureList.QUERY_TILES, ChromeFeatureList.QUERY_TILES_IN_OMNIBOX})
public class OmniboxQueryTileSuggestionTest {
    private static final String SEARCH_URL_PATTERN = "https://www.google.com/search?q=";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private Tab mTab;
    private UrlBar mUrlBar;
    private LocationBarLayout mLocationBar;

    // A convenient wrapper around the real provider, used for matching purposes.
    private TestTileProvider mTileProvider;

    @Before
    public void setUp() {
        loadNative();
        final AtomicBoolean tilesFetched = new AtomicBoolean();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            QueryTileFakeServer.setupFakeServer(2, 4, success -> tilesFetched.set(true));
        });
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        CriteriaHelper.pollUiThread(tilesFetched::get, "Tile fetch was not completed.");

        // Accessing profile needs UI thread.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TileProvider realProvider =
                    TileProviderFactory.getForProfile(Profile.getLastUsedRegularProfile());
            mTileProvider = new TestTileProvider(realProvider);
        });

        mUrlBar = (UrlBar) mActivityTestRule.getActivity().findViewById(R.id.url_bar);
        mLocationBar =
                (LocationBarLayout) mActivityTestRule.getActivity().findViewById(R.id.location_bar);
    }

    @After
    public void tearDown() {}

    /** Tests that tiles show up as expected when omnibox is opened. */
    @Test
    @SmallTest
    public void testShowOmniboxQueryTileSuggestion() throws Exception {
        clickNTPSearchBox();
        OmniboxTestUtils.waitForFocusAndKeyboardActive(mUrlBar, true);
        OmniboxTestUtils.waitForOmniboxSuggestions(mLocationBar);

        Matcher<View> recyclerViewMatcher = withParent(withId(R.id.omnibox_query_tiles));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf());
    }

    /** Tests that clicking on a tile opens up the next level tiles in omnibox. */
    @Test
    @SmallTest
    public void testClickTileOpensNextTierTiles() throws Exception {
        clickNTPSearchBox();
        OmniboxTestUtils.waitForFocusAndKeyboardActive(mUrlBar, true);
        OmniboxTestUtils.waitForOmniboxSuggestions(mLocationBar);

        Matcher<View> recyclerViewMatcher = withParent(withId(R.id.omnibox_query_tiles));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf());

        // Click a tile. Suggestion with next level tiles should show up, and omnibox should show
        // query text from the tile.
        QueryTile tile = mTileProvider.getTileAt(0);
        onView(recyclerViewMatcher).perform(scrollTo(0));
        onView(tileSuggestionMatcher(tile)).perform(click());
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf(0));
        Assert.assertTrue(mUrlBar.getText().toString().contains(tile.queryText));
    }

    /** Tests that clicking on a tile hides the suggestion and shows the query on omnibox. */
    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.QUERY_TILES_ENABLE_QUERY_EDITING)
    public void testClickTileOpensQueryEditMode() throws Exception {
        clickNTPSearchBox();
        OmniboxTestUtils.waitForFocusAndKeyboardActive(mUrlBar, true);
        OmniboxTestUtils.waitForOmniboxSuggestions(mLocationBar);

        Matcher<View> recyclerViewMatcher = withParent(withId(R.id.omnibox_query_tiles));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf());

        // Click the first level tile.
        QueryTile tile = mTileProvider.getTileAt(0);
        QueryTile subtile = mTileProvider.getTileAt(0, 0);
        onView(recyclerViewMatcher).perform(scrollTo(0));
        onView(tileSuggestionMatcher(tile)).perform(click());

        // Click the last level tile. The omnibox should show the query text. No suggestions should
        // show up.
        Assert.assertTrue(mUrlBar.getText().toString().contains(tile.queryText));
        onView(recyclerViewMatcher).perform(scrollTo(0));
        onView(tileSuggestionMatcher(subtile)).perform(click());
        Assert.assertTrue(mUrlBar.getText().toString().contains(subtile.queryText));
        onView(recyclerViewMatcher).check(doesNotExist());
    }

    /** Tests that clicking a last level tile loads the search result page. */
    @Test
    @SmallTest
    public void testClickLastLevelTileOpensSearchResultsPage() throws Exception {
        clickNTPSearchBox();
        OmniboxTestUtils.waitForFocusAndKeyboardActive(mUrlBar, true);
        OmniboxTestUtils.waitForOmniboxSuggestions(mLocationBar);

        Matcher<View> recyclerViewMatcher = withParent(withId(R.id.omnibox_query_tiles));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf());

        // Click twice to get to the search results page.
        QueryTile tile = mTileProvider.getTileAt(0);
        QueryTile subtile = mTileProvider.getTileAt(0, 0);
        onView(recyclerViewMatcher).perform(scrollTo(0));
        onView(tileSuggestionMatcher(tile)).perform(click());
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf(0));
        Assert.assertTrue(mUrlBar.getText().toString().contains(tile.queryText));

        onView(recyclerViewMatcher).perform(scrollTo(0));
        onView(tileSuggestionMatcher(subtile)).perform(click());
        waitForSearchResultsPage();
        Assert.assertTrue(getTabUrl().contains(subtile.queryText));
        onView(recyclerViewMatcher).check(doesNotExist());
    }

    /** Tests that typing on omnibox will dismiss the query tiles suggestion. */
    @Test
    @SmallTest
    public void testTypingInOmniboxDismissesQueryTileSuggestion() throws Exception {
        clickNTPSearchBox();
        OmniboxTestUtils.waitForFocusAndKeyboardActive(mUrlBar, true);
        OmniboxTestUtils.waitForOmniboxSuggestions(mLocationBar);

        Matcher<View> recyclerViewMatcher = withParent(withId(R.id.omnibox_query_tiles));
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf());

        QueryTile tile = mTileProvider.getTileAt(0);
        onView(recyclerViewMatcher).perform(scrollTo(0));
        onView(tileSuggestionMatcher(tile)).perform(click());
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf(0));
        Assert.assertTrue(mUrlBar.getText().toString().contains(tile.queryText));

        // Start editing the text. The query tile suggestion should disappear.
        onView(withId(R.id.url_bar)).perform(replaceText("xyz"));
        waitForOmniboxQueryTileSuggestion(false);

        // Backspace all the way to zero suggest. The query tile suggestion should reappear.
        onView(withId(R.id.url_bar)).perform(clearText());
        matchList(recyclerViewMatcher, mTileProvider.getChildrenOf());
    }

    private Matcher<View> tileSuggestionMatcher(QueryTile tile) {
        Matcher<View> recyclerViewMatcher = withParent(withId(R.id.omnibox_query_tiles));
        return Matchers.allOf(isDescendantOfA(recyclerViewMatcher), withText(tile.displayTitle));
    }

    private void clickNTPSearchBox() {
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

    private void waitForOmniboxQueryTileSuggestion(boolean visible) {
        CriteriaHelper.pollUiThread(() -> {
            View view = mActivityTestRule.getActivity().findViewById(R.id.omnibox_query_tiles);
            if (visible) {
                Criteria.checkThat(view, Matchers.notNullValue());
                Criteria.checkThat(view.getVisibility(), Matchers.is(View.VISIBLE));
            } else {
                if (view == null) return;
                Criteria.checkThat(view.getVisibility(), Matchers.not(View.VISIBLE));
            }
        });
    }

    private void waitForSearchResultsPage() {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("The SRP was never loaded.",
                    ChromeTabUtils.getUrlOnUiThread(mTab).getValidSpecOrEmpty(),
                    Matchers.containsString(SEARCH_URL_PATTERN));
        });
    }

    private void loadNative() {
        final AtomicBoolean mNativeLoaded = new AtomicBoolean();
        final BrowserParts parts = new EmptyBrowserParts() {
            @Override
            public void finishNativeInitialization() {
                mNativeLoaded.set(true);
            }
        };
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            ChromeBrowserInitializer.getInstance().handlePreNativeStartupAndLoadLibraries(parts);
            ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
        });
        CriteriaHelper.pollUiThread(
                () -> mNativeLoaded.get(), "Failed while waiting for starting native.");
    }
}
