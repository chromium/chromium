// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.res.Resources;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageView;
import org.chromium.chrome.browser.ntp.snippets.CategoryInt;
import org.chromium.chrome.browser.ntp.snippets.CategoryStatus;
import org.chromium.chrome.browser.ntp.snippets.ContentSuggestionsCardLayout;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.suggestions.ContentSuggestionsAdditionalAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.test.util.UiRestriction;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for {@link NewTabPageRecyclerView}.
 */
// TODO(https://crbug.com/894334): Remove format suppression once formatting bug is fixed.
// clang-format off
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Features.DisableFeatures(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)
@RetryOnFailure
public class NewTabPageRecyclerViewTest {
    // clang-format on

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    private static final String TEST_PAGE = "/chrome/test/data/android/navigate/simple.html";
    private static final long FAKE_PUBLISH_TIMESTAMP = 1466614774;
    private static final long FAKE_FETCH_TIMESTAMP = 1466634774;
    private static final float FAKE_SNIPPET_SCORE = 10f;

    // TODO(dgn): Properly bypass the native code when testing with a fake suggestions source.
    // We currently mix the fake and the snippets bridge, resulting in crashes with unregistered
    // categories.
    @CategoryInt
    private static final int TEST_CATEGORY = KnownCategories.ARTICLES;

    private Tab mTab;
    private NewTabPage mNtp;
    private EmbeddedTestServer mTestServer;
    private FakeSuggestionsSource mSource;

    @Before
    public void setUp() {
        mTestServer = EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());

        FakeMostVisitedSites mostVisitedSites = new FakeMostVisitedSites();
        mostVisitedSites.setTileSuggestions(mTestServer.getURL(TEST_PAGE));
        mSuggestionsDeps.getFactory().mostVisitedSites = mostVisitedSites;

        mSource = new FakeSuggestionsSource();
        mSource.setInfoForCategory(TEST_CATEGORY,
                new SuggestionsCategoryInfo(TEST_CATEGORY, "Suggestions test title",
                        ContentSuggestionsCardLayout.FULL_CARD,
                        ContentSuggestionsAdditionalAction.FETCH, /*showIfEmpty=*/true,
                        "noSuggestionsMessage"));

        // Set the status as AVAILABLE so no spinner is shown. Showing the spinner during
        // initialization can cause the test to hang because the message queue never becomes idle.
        mSource.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);

        mSuggestionsDeps.getFactory().suggestionsSource = mSource;

        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        assertTrue(mTab.getNativePage() instanceof NewTabPage);
        mNtp = (NewTabPage) mTab.getNativePage();

        // When scrolling to a View, we wait until the View is no longer updating - when it is no
        // longer dirty. If scroll to load is triggered, the animated progress spinner will keep
        // the RecyclerView dirty as it is constantly updating.
        //
        // We do not want to disable the Scroll to Load feature entirely because its presence
        // effects other elements of the UI - it moves the Learn More link into the Context Menu.
        // Removing the ScrollToLoad listener from the RecyclerView allows us to prevent scroll to
        // load triggering while maintaining the UI otherwise.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mNtp.getNewTabPageView().getRecyclerView().clearScrollToLoadListener());
    }

    @After
    public void tearDown() {
        mTestServer.stopAndDestroyServer();
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testClickSuggestion() {
        setSuggestionsAndWaitForUpdate(10);
        List<SnippetArticle> suggestions = mSource.getSuggestionsForCategory(TEST_CATEGORY);

        // Scroll the last suggestion into view and click it.
        SnippetArticle suggestion = suggestions.get(suggestions.size() - 1);
        int suggestionPosition = getLastCardPosition();
        final View suggestionView = getViewHolderAtPosition(suggestionPosition).itemView;
        ChromeTabUtils.waitForTabPageLoaded(mTab, suggestion.mUrl, () -> {
            TestTouchUtils.performClickOnMainSync(
                    InstrumentationRegistry.getInstrumentation(), suggestionView);
        });
        assertEquals(suggestion.mUrl, mTab.getUrl());
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testAllDismissed() throws TimeoutException {
        setSuggestionsAndWaitForUpdate(3);
        assertEquals(3, mSource.getSuggestionsForCategory(TEST_CATEGORY).size());
        assertEquals(1, mSource.getCategories().length);
        assertEquals(TEST_CATEGORY, mSource.getCategories()[0]);

        // Dismiss the sign in promo.
        int signinPromoPosition = getAdapter().getFirstPositionForType(ItemViewType.PROMO);
        dismissItemAtPosition(signinPromoPosition);

        // Dismiss all the cards. Then, we are left with the status card,
        // which shouldn't be dismissible.
        while (true) {
            int cardPosition = getAdapter().getFirstCardPosition();
            if (cardPosition == RecyclerView.NO_POSITION) break;
            final ViewHolder viewHolder = getViewHolderAtPosition(cardPosition);
            if (viewHolder.getItemViewType() == ItemViewType.STATUS) {
                assertFalse(((NewTabPageViewHolder) viewHolder).isDismissable());
                break;
            }
            dismissItemAtPosition(cardPosition);
        }
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    public void testDismissArticleWithContextMenu() throws Exception {
        setSuggestionsAndWaitForUpdate(10);
        List<SnippetArticle> suggestions = mSource.getSuggestionsForCategory(TEST_CATEGORY);
        assertEquals(10, suggestions.size());

        // Scroll a suggestion into view.
        int suggestionPosition = getLastCardPosition();
        View suggestionView = getViewHolderAtPosition(suggestionPosition).itemView;

        // Dismiss the suggestion using the context menu.
        invokeContextMenu(suggestionView, ContextMenuManager.ContextMenuItemId.REMOVE);
        RecyclerViewTestUtils.waitForViewToDetach(getRecyclerView(), suggestionView);

        suggestions = mSource.getSuggestionsForCategory(TEST_CATEGORY);
        assertEquals(9, suggestions.size());
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE})
    public void testSnapScroll() {
        setSuggestionsAndWaitForUpdate(0);

        Resources resources = InstrumentationRegistry.getTargetContext().getResources();
        int toolbarHeight = resources.getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                + resources.getDimensionPixelSize(R.dimen.toolbar_progress_bar_height);
        View searchBox = getNtpView().findViewById(R.id.search_box);
        int searchBoxTop = searchBox.getTop() + searchBox.getPaddingTop();
        int searchBoxTransitionLength =
                resources.getDimensionPixelSize(R.dimen.ntp_search_box_transition_length);

        // Two different snapping regions: snapping back up to the watershed point in the middle,
        // snapping forward after that.
        assertEquals(0, getSnapPosition(0));
        assertEquals(0, getSnapPosition(toolbarHeight / 2 - 1));
        assertEquals(toolbarHeight, getSnapPosition(toolbarHeight / 2));
        assertEquals(toolbarHeight, getSnapPosition(toolbarHeight));
        assertEquals(toolbarHeight + 1, getSnapPosition(toolbarHeight + 1));

        assertEquals(searchBoxTop - searchBoxTransitionLength - 1,
                getSnapPosition(searchBoxTop - searchBoxTransitionLength - 1));
        assertEquals(searchBoxTop - searchBoxTransitionLength,
                getSnapPosition(searchBoxTop - searchBoxTransitionLength));
        assertEquals(searchBoxTop - searchBoxTransitionLength,
                getSnapPosition(searchBoxTop - searchBoxTransitionLength / 2 - 1));
        assertEquals(searchBoxTop, getSnapPosition(searchBoxTop - searchBoxTransitionLength / 2));
        assertEquals(searchBoxTop, getSnapPosition(searchBoxTop));
        assertEquals(searchBoxTop + 1, getSnapPosition(searchBoxTop + 1));
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage"})
    @Restriction({UiRestriction.RESTRICTION_TYPE_TABLET})
    public void testSnapScroll_tablet() {
        setSuggestionsAndWaitForUpdate(0);

        Resources res = InstrumentationRegistry.getTargetContext().getResources();
        int toolbarHeight = res.getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                + res.getDimensionPixelSize(R.dimen.toolbar_progress_bar_height);
        View searchBox = getNtpView().findViewById(R.id.search_box);
        int searchBoxTop = searchBox.getTop() + searchBox.getPaddingTop();
        int searchBoxTransitionLength =
                res.getDimensionPixelSize(R.dimen.ntp_search_box_transition_length);

        // No snapping on tablets.
        // Note: This ignores snapping for the peeking cards, which is currently disabled
        // by default.
        assertEquals(0, getSnapPosition(0));
        assertEquals(toolbarHeight / 2 - 1, getSnapPosition(toolbarHeight / 2 - 1));
        assertEquals(toolbarHeight / 2, getSnapPosition(toolbarHeight / 2));
        assertEquals(toolbarHeight, getSnapPosition(toolbarHeight));
        assertEquals(toolbarHeight + 1, getSnapPosition(toolbarHeight + 1));

        assertEquals(searchBoxTop - searchBoxTransitionLength - 1,
                getSnapPosition(searchBoxTop - searchBoxTransitionLength - 1));
        assertEquals(searchBoxTop - searchBoxTransitionLength,
                getSnapPosition(searchBoxTop - searchBoxTransitionLength));
        assertEquals(searchBoxTop - searchBoxTransitionLength / 2 - 1,
                getSnapPosition(searchBoxTop - searchBoxTransitionLength / 2 - 1));
        assertEquals(searchBoxTop - searchBoxTransitionLength / 2,
                getSnapPosition(searchBoxTop - searchBoxTransitionLength / 2));
        assertEquals(searchBoxTop, getSnapPosition(searchBoxTop));
        assertEquals(searchBoxTop + 1, getSnapPosition(searchBoxTop + 1));
    }

    private int getSnapPosition(int scrollPosition) {
        NewTabPageView ntpView = getNtpView();
        return ntpView.getSnapScrollHelper().calculateSnapPosition(scrollPosition);
    }

    private NewTabPageView getNtpView() {
        return mNtp.getNewTabPageView();
    }

    private NewTabPageRecyclerView getRecyclerView() {
        return getNtpView().getRecyclerView();
    }

    private NewTabPageAdapter getAdapter() {
        return getRecyclerView().getNewTabPageAdapter();
    }

    private int getLastCardPosition() {
        int count = getAdapter().getItemCount();
        for (int i = count - 1; i >= 0; i--) {
            if (getAdapter().getItemViewType(i) == ItemViewType.SNIPPET) return i;
        }
        return RecyclerView.NO_POSITION;
    }

    /**
     * Scroll the {@link View} at the given adapter position into view and returns
     * its {@link ViewHolder}.
     * @param position the adapter position for which to return the {@link ViewHolder}.
     * @return the ViewHolder for the given {@code position}.
     */
    private ViewHolder getViewHolderAtPosition(final int position) {
        final NewTabPageRecyclerView recyclerView = getRecyclerView();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            recyclerView.getLinearLayoutManager().scrollToPositionWithOffset(position,
                    mActivityTestRule.getActivity().getResources().getDimensionPixelSize(
                            R.dimen.tab_strip_height));
        });
        return RecyclerViewTestUtils.waitForView(getRecyclerView(), position);
    }

    /**
     * Dismiss the item at the given {@code position} and wait until it has been removed from the
     * {@link RecyclerView}.
     * @param position the adapter position to remove.
     * @throws TimeoutException
     */
    private void dismissItemAtPosition(int position) throws TimeoutException {
        final ViewHolder viewHolder = getViewHolderAtPosition(position);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { getRecyclerView().dismissItemWithAnimation(viewHolder); });
        RecyclerViewTestUtils.waitForViewToDetach(getRecyclerView(), (viewHolder.itemView));
    }

    private void setSuggestionsAndWaitForUpdate(final int suggestionsCount) {
        final FakeSuggestionsSource source = mSource;

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            source.setStatusForCategory(TEST_CATEGORY, CategoryStatus.AVAILABLE);
            source.setSuggestionsForCategory(TEST_CATEGORY, buildSuggestions(suggestionsCount));
        });
        RecyclerViewTestUtils.waitForStableRecyclerView(getRecyclerView());
    }

    private List<SnippetArticle> buildSuggestions(int suggestionsCount) {
        List<SnippetArticle> suggestions = new ArrayList<>();
        for (int i = 0; i < suggestionsCount; i++) {
            String url = mTestServer.getURL(TEST_PAGE) + "#" + i;
            suggestions.add(new SnippetArticle(TEST_CATEGORY, "id" + i, "title" + i,
                    "publisher" + i, url, FAKE_PUBLISH_TIMESTAMP + i, FAKE_SNIPPET_SCORE,
                    FAKE_FETCH_TIMESTAMP, false, /* thumbnailDominantColor = */ null));
        }
        return suggestions;
    }

    private void invokeContextMenu(View view, int contextMenuItemId) throws ExecutionException {
        TestTouchUtils.performLongClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), view);
        assertTrue(InstrumentationRegistry.getInstrumentation().invokeContextMenuAction(
                mActivityTestRule.getActivity(), contextMenuItemId, 0));
    }

    private static void assertArrayEquals(int[] expected, int[] actual) {
        assertEquals(Arrays.toString(expected), Arrays.toString(actual));
    }
}
