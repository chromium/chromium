// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.mostvisited;

import android.support.test.InstrumentationRegistry;
import android.view.KeyEvent;
import android.view.View;

import androidx.recyclerview.widget.RecyclerView.LayoutManager;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.LocationBarLayout;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils.TestAutocompleteController;
import org.chromium.chrome.test.util.WaitForFocusHelper;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.omnibox.AutocompleteMatch.NavsuggestTile;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.content_public.browser.test.util.KeyUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.url.GURL;

import java.util.Arrays;

/**
 * Tests of the Most Visited Tiles.
 * TODO(ender): add keyboard navigation for MV tiles once we can use real AutocompleteController.
 * The TestAutocompleteController is unable to properly classify the synthetic OmniboxSuggestions
 * and issuing KEYCODE_ENTER results in no action.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class MostVisitedTilesTest {
    // A dummy URL used in the Omnibox for factual correctness.
    // The MV tiles are meant to be shown when the user is currently on any website.
    // Note: since we use the TestAutocompleteController, this could be any string.
    private static final String PAGE_URL = "chrome://version";
    private static final String SEARCH_QUERY = "related search query";

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();
    @ClassRule
    public static EmbeddedTestServerRule sTestServerRule = new EmbeddedTestServerRule();

    @ClassRule
    public static DisableAnimationsTestRule sNoAnimationsRule = new DisableAnimationsTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private ChromeTabbedActivity mActivity;
    private UrlBar mUrlBar;
    private LocationBarLayout mLocationBarLayout;
    private TestAutocompleteController mController;
    private AutocompleteCoordinator mAutocomplete;
    private EmbeddedTestServer mTestServer;
    private Tab mTab;
    private BaseCarouselSuggestionView mCarousel;

    private NavsuggestTile mTile1;
    private NavsuggestTile mTile2;
    private NavsuggestTile mTile3;

    @Before
    public void setUp() throws Exception {
        sActivityTestRule.waitForActivityNativeInitializationComplete();
        mActivity = sActivityTestRule.getActivity();
        mLocationBarLayout = mActivity.findViewById(R.id.location_bar);
        mUrlBar = mActivity.findViewById(R.id.url_bar);
        mAutocomplete = mLocationBarLayout.getAutocompleteCoordinator();
        mTab = mActivity.getActivityTab();

        ChromeTabUtils.waitForInteractable(mTab);
        ChromeTabUtils.loadUrlOnUiThread(mTab, PAGE_URL);
        ChromeTabUtils.waitForTabPageLoaded(mTab, null);

        // Set up a fake AutocompleteController that will supply the suggestions.
        mController = new OmniboxTestUtils.TestAutocompleteController(
                mAutocomplete.getSuggestionsReceivedListenerForTest());
        mAutocomplete.setAutocompleteControllerForTest(mController);

        setUpSuggestionsToShow();
        focusOmniboxAndWaitForSuggestions();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        mCarousel = OmniboxTestUtils.getSuggestionViewAtPosition(mLocationBarLayout, 1);
    }

    /**
     * Initialize a small set of suggestions resembling what the user would see while visiting an
     * URL.
     */
    private void setUpSuggestionsToShow() {
        // Set up basic AutocompleteResult hosting a MostVisitedTiles suggestion.
        mTestServer = sTestServerRule.getServer();
        mTile1 = new NavsuggestTile(
                "About", new GURL(mTestServer.getURL("/chrome/test/data/android/about.html")));
        mTile2 = new NavsuggestTile(
                "Happy Server", new GURL(mTestServer.getURL("/chrome/test/data/android/ok.txt")));
        mTile3 = new NavsuggestTile(
                "Test Server", new GURL(mTestServer.getURL("/chrome/test/data/android/test.html")));

        AutocompleteResult autocompleteResult = new AutocompleteResult(null, null);
        AutocompleteMatchBuilder builder = new AutocompleteMatchBuilder();

        // First suggestion is the current content of the Omnibox.
        builder.setType(OmniboxSuggestionType.NAVSUGGEST);
        builder.setDisplayText(PAGE_URL);
        builder.setUrl(new GURL(PAGE_URL));
        autocompleteResult.getSuggestionsList().add(builder.build());
        builder.reset();

        // Second suggestion is the MV Tiles.
        builder.setType(OmniboxSuggestionType.TILE_NAVSUGGEST);
        builder.setNavsuggestTiles(Arrays.asList(new NavsuggestTile[] {mTile1, mTile2, mTile3}));
        autocompleteResult.getSuggestionsList().add(builder.build());
        builder.reset();

        // Third suggestion - search query with a header.
        builder.setType(OmniboxSuggestionType.SEARCH_SUGGEST);
        builder.setDisplayText(SEARCH_QUERY);
        builder.setIsSearch(true);
        builder.setGroupId(1);
        autocompleteResult.getSuggestionsList().add(builder.build());
        builder.reset();

        autocompleteResult.getGroupsDetails().put(
                1, new AutocompleteResult.GroupDetails("See also", false));

        mController.addAutocompleteResult(PAGE_URL, PAGE_URL, autocompleteResult);
    }

    private void focusOmniboxAndWaitForSuggestions() {
        ChromeTabUtils.waitForInteractable(mTab);
        WaitForFocusHelper.acquireFocusForView(mUrlBar);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Note: ignore the keyboard state, as keyboard might be late even with animations disabled.
        CriteriaHelper.pollInstrumentationThread(() -> {
            Criteria.checkThat("URL Bar did not have expected focus",
                    OmniboxTestUtils.doesUrlBarHaveFocus(mUrlBar), Matchers.is(true));
        });
        // Make sure the suggestions are shown on screen.
        OmniboxTestUtils.waitForOmniboxSuggestions(mLocationBarLayout);
    }

    private void clickTileAtPosition(int position) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            LayoutManager manager = mCarousel.getRecyclerViewForTest().getLayoutManager();
            Assert.assertTrue(position < manager.getItemCount());
            manager.scrollToPosition(position);
            View view = manager.findViewByPosition(position);
            Assert.assertNotNull(view);
            view.performClick();
        });
    }

    /**
     * Send key event to the Application.
     * @param keyCode Key code associated with the Key event.
     */
    private void sendKey(final int keyCode) {
        KeyUtils.singleKeyEventActivity(
                InstrumentationRegistry.getInstrumentation(), mActivity, keyCode);
    }

    private void checkUrlBarTextIs(final String text) {
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mUrlBar.getTextWithoutAutocomplete(), Matchers.equalTo(text));
        }, 300, 50);
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxMostVisitedTiles")
    public void keyboardNavigation_highlightingNextTileUpdatesUrlBarText()
            throws InterruptedException {
        // Skip past the 'what-you-typed' suggestion.
        sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        checkUrlBarTextIs(mTile1.url.getSpec());

        sendKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        checkUrlBarTextIs(mTile2.url.getSpec());

        sendKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        checkUrlBarTextIs(mTile3.url.getSpec());

        // Note: the carousel does not wrap around.
        sendKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        checkUrlBarTextIs(mTile3.url.getSpec());
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxMostVisitedTiles")
    public void keyboardNavigation_highlightingPreviousTileUpdatesUrlBarText()
            throws InterruptedException {
        // Skip past the 'what-you-typed' suggestion.
        sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        checkUrlBarTextIs(mTile1.url.getSpec());

        sendKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        checkUrlBarTextIs(mTile2.url.getSpec());

        sendKey(KeyEvent.KEYCODE_DPAD_LEFT);
        checkUrlBarTextIs(mTile1.url.getSpec());

        // Note: the carousel does not wrap around.
        sendKey(KeyEvent.KEYCODE_DPAD_LEFT);
        checkUrlBarTextIs(mTile1.url.getSpec());
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxMostVisitedTiles")
    public void keyboardNavigation_highlightAlwaysStartsWithFirstElement()
            throws InterruptedException {
        // Skip past the 'what-you-typed' suggestion.
        sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        checkUrlBarTextIs(mTile1.url.getSpec());

        sendKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        checkUrlBarTextIs(mTile2.url.getSpec());

        sendKey(KeyEvent.KEYCODE_DPAD_RIGHT);
        checkUrlBarTextIs(mTile3.url.getSpec());

        // Move to the search suggestion skipping the header.
        sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        sendKey(KeyEvent.KEYCODE_DPAD_DOWN);
        checkUrlBarTextIs(SEARCH_QUERY);

        // Move back to the MV Tiles. Observe that the first element is again highlighted.
        sendKey(KeyEvent.KEYCODE_DPAD_UP);
        sendKey(KeyEvent.KEYCODE_DPAD_UP);
        checkUrlBarTextIs(mTile1.url.getSpec());
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxMostVisitedTiles")
    public void touchNavigation_clickOnFirstMVTile() throws Exception {
        clickTileAtPosition(0);
        ChromeTabUtils.waitForTabPageLoaded(mTab, mTile1.url.getSpec());
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxMostVisitedTiles")
    public void touchNavigation_clickOnMiddleMVTile() throws Exception {
        clickTileAtPosition(1);
        ChromeTabUtils.waitForTabPageLoaded(mTab, mTile2.url.getSpec());
    }

    @Test
    @MediumTest
    @EnableFeatures("OmniboxMostVisitedTiles")
    public void touchNavigation_clickOnLastMVTile() throws Exception {
        clickTileAtPosition(2);
        ChromeTabUtils.waitForTabPageLoaded(mTab, mTile3.url.getSpec());
    }
}
