// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.support.test.InstrumentationRegistry;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.explore_sites.ExploreSitesBridge;
import org.chromium.chrome.browser.explore_sites.ExploreSitesCategory;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.ViewUtils;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for {@link TileGroup} on the New Tab Page.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TileGroupTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, true);

    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    private static final String[] FAKE_MOST_VISITED_URLS =
            new String[] {"/chrome/test/data/android/navigate/one.html",
                    "/chrome/test/data/android/navigate/two.html",
                    "/chrome/test/data/android/navigate/three.html"};

    private NewTabPage mNtp;
    private String[] mSiteSuggestionUrls;
    private FakeMostVisitedSites mMostVisitedSites;
    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        mSiteSuggestionUrls = mTestServer.getURLs(FAKE_MOST_VISITED_URLS);

        mMostVisitedSites = new FakeMostVisitedSites();
        mSuggestionsDeps.getFactory().mostVisitedSites = mMostVisitedSites;
        mMostVisitedSites.setTileSuggestions(mSiteSuggestionUrls);
    }

    public void initializeTab() {
        sActivityTestRule.loadUrl(UrlConstants.NTP_URL);
        Tab mTab = sActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        Assert.assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();

        ViewUtils.waitForView(
                (ViewGroup) mNtp.getView(), ViewMatchers.withId(R.id.tile_grid_layout));
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testDismissTileWithContextMenu() throws Exception {
        initializeTab();
        SiteSuggestion siteToDismiss = mMostVisitedSites.getCurrentSites().get(0);
        final View tileView = getTileViewFor(siteToDismiss);

        // Dismiss the tile using the context menu.
        invokeContextMenu(tileView, ContextMenuManager.ContextMenuItemId.REMOVE);
        Assert.assertTrue(mMostVisitedSites.isUrlBlocklisted(new GURL(mSiteSuggestionUrls[0])));

        // Ensure that the removal is reflected in the ui.
        Assert.assertEquals(3, getTileGridLayout().getChildCount());
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMostVisitedSites.setTileSuggestions(mSiteSuggestionUrls[1], mSiteSuggestionUrls[2]);
        });
        waitForTileRemoved(siteToDismiss);
        Assert.assertEquals(2, getTileGridLayout().getChildCount());
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testDismissExploreTileWithContextMenuFails() throws Exception {
        SiteSuggestion exploreTile = recreateSuggestionsWithExploreTile();

        initializeTab();

        Assert.assertEquals(4, getTileGridLayout().getChildCount());

        final View tileView = getTileViewFor(exploreTile);
        TestTouchUtils.performLongClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), tileView);
        Assert.assertFalse(InstrumentationRegistry.getInstrumentation().invokeContextMenuAction(
                sActivityTestRule.getActivity(), ContextMenuManager.ContextMenuItemId.REMOVE, 0));
        Assert.assertEquals(4, getTileGridLayout().getChildCount());
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testDismissTileUndo() throws Exception {
        initializeTab();
        GURL url0 = new GURL(mSiteSuggestionUrls[0]);
        GURL url1 = new GURL(mSiteSuggestionUrls[1]);
        GURL url2 = new GURL(mSiteSuggestionUrls[2]);
        SiteSuggestion siteToDismiss = mMostVisitedSites.getCurrentSites().get(0);
        final ViewGroup tileContainer = getTileGridLayout();
        final View tileView = getTileViewFor(siteToDismiss);
        Assert.assertEquals(3, tileContainer.getChildCount());

        // Dismiss the tile using the context menu.
        invokeContextMenu(tileView, ContextMenuManager.ContextMenuItemId.REMOVE);

        // Ensure that the removal update goes through.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mMostVisitedSites.setTileSuggestions(mSiteSuggestionUrls[1], mSiteSuggestionUrls[2]);
        });
        waitForTileRemoved(siteToDismiss);
        Assert.assertEquals(2, tileContainer.getChildCount());
        final View snackbarButton = waitForSnackbar(sActivityTestRule.getActivity());

        Assert.assertTrue(mMostVisitedSites.isUrlBlocklisted(url0));
        TestThreadUtils.runOnUiThreadBlocking(() -> { snackbarButton.callOnClick(); });

        Assert.assertFalse(mMostVisitedSites.isUrlBlocklisted(url0));

        // Ensure that the removal of the update goes through.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mMostVisitedSites.setTileSuggestions(mSiteSuggestionUrls); });
        waitForTileAdded(siteToDismiss);
        Assert.assertEquals(3, tileContainer.getChildCount());
    }

    private TileGridLayout getTileGridLayout() {
        ViewGroup newTabPageLayout = mNtp.getNewTabPageLayout();
        Assert.assertNotNull("Unable to retrieve the NewTabPageLayout.", newTabPageLayout);

        TileGridLayout tileGridLayout = newTabPageLayout.findViewById(R.id.tile_grid_layout);
        Assert.assertNotNull("Unable to retrieve the TileGridLayout.", tileGridLayout);
        return tileGridLayout;
    }

    private View getTileViewFor(SiteSuggestion suggestion) {
        View tileView = getTileGridLayout().getTileView(suggestion);
        Assert.assertNotNull("Tile not found for suggestion " + suggestion.url, tileView);

        return tileView;
    }

    private void invokeContextMenu(View view, int contextMenuItemId) throws ExecutionException {
        TestTouchUtils.performLongClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), view);
        Assert.assertTrue(InstrumentationRegistry.getInstrumentation().invokeContextMenuAction(
                sActivityTestRule.getActivity(), contextMenuItemId, 0));
    }

    /** Wait for the snackbar associated to a tile dismissal to be shown and returns its button. */
    private static View waitForSnackbar(final ChromeActivity activity) {
        final String expectedSnackbarMessage =
                activity.getResources().getString(R.string.most_visited_item_removed);
        CriteriaHelper.pollUiThread(() -> {
            SnackbarManager snackbarManager = activity.getSnackbarManager();
            Criteria.checkThat(snackbarManager.isShowing(), Matchers.is(true));
            TextView snackbarMessage = (TextView) activity.findViewById(R.id.snackbar_message);
            Criteria.checkThat(snackbarMessage, Matchers.notNullValue());
            Criteria.checkThat(
                    snackbarMessage.getText().toString(), Matchers.is(expectedSnackbarMessage));
        });

        return activity.findViewById(R.id.snackbar_button);
    }

    private void waitForTileRemoved(final SiteSuggestion suggestion) throws TimeoutException {
        TileGridLayout tileContainer = getTileGridLayout();
        final SuggestionsTileView removedTile = tileContainer.getTileView(suggestion);
        if (removedTile == null) return;

        final CallbackHelper callback = new CallbackHelper();
        tileContainer.setOnHierarchyChangeListener(new ViewGroup.OnHierarchyChangeListener() {
            @Override
            public void onChildViewAdded(View parent, View child) {}

            @Override
            public void onChildViewRemoved(View parent, View child) {
                if (child == removedTile) callback.notifyCalled();
            }
        });
        callback.waitForCallback("The expected tile was not removed.", 0);
        tileContainer.setOnHierarchyChangeListener(null);
    }

    private void waitForTileAdded(final SiteSuggestion suggestion) throws TimeoutException {
        TileGridLayout tileContainer = getTileGridLayout();
        if (tileContainer.getTileView(suggestion) != null) return;

        final CallbackHelper callback = new CallbackHelper();
        tileContainer.setOnHierarchyChangeListener(new ViewGroup.OnHierarchyChangeListener() {
            @Override
            public void onChildViewAdded(View parent, View child) {
                if (!(child instanceof SuggestionsTileView)) return;
                if (!((SuggestionsTileView) child).getData().equals(suggestion)) return;

                callback.notifyCalled();
            }

            @Override
            public void onChildViewRemoved(View parent, View child) {}
        });
        callback.waitForCallback("The expected tile was not added.", 0);
        tileContainer.setOnHierarchyChangeListener(null);
    }

    private SiteSuggestion recreateSuggestionsWithExploreTile() {
        // need a copy of the list in order to modify it.
        final ArrayList<SiteSuggestion> currentSuggestions =
                new ArrayList<>(mMostVisitedSites.getCurrentSites());

        SiteSuggestion exploreTile = new SiteSuggestion("chrome-native://explore",
                new GURL("chrome-native://explore"), "", TileTitleSource.UNKNOWN,
                TileSource.EXPLORE, TileSectionType.PERSONALIZED, new Date());
        currentSuggestions.add(exploreTile);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mMostVisitedSites.setTileSuggestions(currentSuggestions));

        // Set up ExploreSitesBridge for testing.
        List<ExploreSitesCategory> category = new ArrayList<>();
        category.add(new ExploreSitesCategory(0, 1, "foo", 0, 0));
        ExploreSitesBridge.setCatalogForTesting(category);

        return exploreTile;
    }
}
