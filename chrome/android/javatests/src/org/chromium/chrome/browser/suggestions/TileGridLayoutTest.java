// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static junit.framework.Assert.assertEquals;
import static junit.framework.Assert.assertNotNull;
import static junit.framework.Assert.assertTrue;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.greaterThan;

import static org.chromium.chrome.test.util.browser.suggestions.FakeMostVisitedSites.createSiteSuggestion;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.support.annotation.Nullable;
import android.support.test.filters.MediumTest;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.UrlConstants;
import org.chromium.chrome.browser.modelutil.ListObservable;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.cards.NewTabPageViewHolder.PartialBindCallback;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.util.ViewUtils;
import org.chromium.chrome.browser.widget.displaystyle.UiConfig;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.RenderTestRule;
import org.chromium.chrome.test.util.browser.RecyclerViewTestUtils;
import org.chromium.chrome.test.util.browser.offlinepages.FakeOfflinePageBridge;
import org.chromium.chrome.test.util.browser.suggestions.FakeMostVisitedSites;
import org.chromium.chrome.test.util.browser.suggestions.FakeSuggestionsSource;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for the {@link TileGridLayout} on the New Tab Page.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class TileGridLayoutTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public RenderTestRule mRenderTestRule = new RenderTestRule();

    private static final String[] FAKE_MOST_VISITED_URLS = new String[] {
            "/chrome/test/data/android/navigate/one.html",
            "/chrome/test/data/android/navigate/two.html",
            "/chrome/test/data/android/navigate/three.html",
            "/chrome/test/data/android/navigate/four.html",
            "/chrome/test/data/android/navigate/five.html",
            "/chrome/test/data/android/navigate/six.html",
            "/chrome/test/data/android/navigate/seven.html",
            "/chrome/test/data/android/navigate/eight.html",
            "/chrome/test/data/android/navigate/nine.html",
    };

    private static final String[] FAKE_MOST_VISITED_TITLES =
            new String[] {"ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE"};

    private final CallbackHelper mLoadCompleteHelper = new CallbackHelper();

    @Test
    @MediumTest
    @Feature({"NewTabPage", "RenderTest"})
    public void testTileGridAppearance() throws Exception {
        NewTabPage ntp = setUpFakeDataToShowOnNtp(FAKE_MOST_VISITED_URLS.length);
        mRenderTestRule.render(getTileGridLayout(ntp), "ntp_tile_grid_layout");
    }

    @Test
    //@MediumTest
    @DisabledTest(message = "crbug.com/771648")
    @Feature({"NewTabPage", "RenderTest"})
    public void testModernTileGridAppearance_Full() throws IOException, InterruptedException {
        View tileGridLayout = renderTiles(makeSuggestions(FAKE_MOST_VISITED_URLS.length));

        setOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT, mActivityTestRule.getActivity());
        mRenderTestRule.render(tileGridLayout, "modern_full_grid_portrait");

        setOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, mActivityTestRule.getActivity());
        mRenderTestRule.render(tileGridLayout, "modern_full_grid_landscape");

        // In landscape, modern tiles should use all available space.
        int tileGridMaxWidthPx = tileGridLayout.getResources().getDimensionPixelSize(
                R.dimen.tile_grid_layout_max_width);
        if (((FrameLayout) tileGridLayout.getParent()).getMeasuredWidth() > tileGridMaxWidthPx) {
            assertThat(tileGridLayout.getMeasuredWidth(), greaterThan(tileGridMaxWidthPx));
        }
    }

    @Test
    //@MediumTest
    @DisabledTest(message = "crbug.com/771648")
    @RetryOnFailure
    @Feature({"NewTabPage", "RenderTest"})
    public void testModernTileGridAppearance_Two() throws IOException, InterruptedException {
        View tileGridLayout = renderTiles(makeSuggestions(2));

        setOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT, mActivityTestRule.getActivity());
        mRenderTestRule.render(tileGridLayout, "modern_two_tiles_grid_portrait");

        setOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE, mActivityTestRule.getActivity());
        mRenderTestRule.render(tileGridLayout, "modern_two_tiles_grid_landscape");
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "RenderTest"})
    public void testTileAppearanceModern()
            throws IOException, InterruptedException, TimeoutException {
        List<SiteSuggestion> suggestions = makeSuggestions(2);
        List<String> offlineAvailableUrls = Collections.singletonList(suggestions.get(0).url);
        ViewGroup tiles = renderTiles(suggestions, offlineAvailableUrls);

        mLoadCompleteHelper.waitForCallback(0);

        mRenderTestRule.render(tiles.getChildAt(0), "tile_modern_offline");
        mRenderTestRule.render(tiles.getChildAt(1), "tile_modern");
    }

    private List<SiteSuggestion> makeSuggestions(int count) {
        List<SiteSuggestion> siteSuggestions = new ArrayList<>(count);

        assertEquals(FAKE_MOST_VISITED_URLS.length, FAKE_MOST_VISITED_TITLES.length);
        assertTrue(count <= FAKE_MOST_VISITED_URLS.length);

        for (int i = 0; i < count; i++) {
            String url = mTestServerRule.getServer().getURL(FAKE_MOST_VISITED_URLS[i]);
            siteSuggestions.add(createSiteSuggestion(FAKE_MOST_VISITED_TITLES[i], url));
        }

        return siteSuggestions;
    }

    private NewTabPage setUpFakeDataToShowOnNtp(int suggestionCount) throws InterruptedException {
        List<SiteSuggestion> siteSuggestions = makeSuggestions(suggestionCount);

        FakeMostVisitedSites mMostVisitedSites = new FakeMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(siteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mMostVisitedSites;

        mSuggestionsDeps.getFactory().suggestionsSource = new FakeSuggestionsSource();

        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);

        Tab mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);

        assertTrue(mTab.getNativePage() instanceof NewTabPage);
        NewTabPage ntp = (NewTabPage) mTab.getNativePage();

        RecyclerViewTestUtils.waitForStableRecyclerView(ntp.getNewTabPageView().getRecyclerView());
        return ntp;
    }

    private void setOrientation(final int requestedOrientation, final Activity activity) {
        if (orientationMatchesRequest(activity, requestedOrientation)) return;

        ThreadUtils.runOnUiThreadBlocking(
                () -> activity.setRequestedOrientation(requestedOrientation));

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return orientationMatchesRequest(activity, requestedOrientation);
            }
        });
    }

    /**
     * Checks whether the requested orientation matches the current one.
     * @param activity Activity to check the orientation from. We pull its {@link Configuration} and
     *         content {@link View}.
     * @param requestedOrientation The requested orientation, as used in
     *         {@link ActivityInfo#screenOrientation}.
     */
    private boolean orientationMatchesRequest(Activity activity, int requestedOrientation) {
        // Note: Requests use a constant from ActivityInfo, not Configuration.ORIENTATION_*!
        boolean expectLandscape = requestedOrientation == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE;

        // We check the orientation by looking at the dimensions of the content view. Looking at
        // orientation from the configuration is not reliable as sometimes the activity gets the
        // event that its configuration changed, but has not updated its layout yet.
        Configuration configuration = activity.getResources().getConfiguration();
        View contentView = activity.findViewById(android.R.id.content);
        int smallestWidthPx = ViewUtils.dpToPx(activity, configuration.smallestScreenWidthDp);
        boolean viewIsLandscape = contentView.getMeasuredWidth() > smallestWidthPx;

        return expectLandscape == viewIsLandscape;
    }

    private TileGridLayout getTileGridLayout(NewTabPage ntp) {
        TileGridLayout tileGridLayout = ntp.getNewTabPageView().findViewById(R.id.tile_grid_layout);
        assertNotNull("Unable to retrieve the TileGridLayout.", tileGridLayout);
        return tileGridLayout;
    }

    /**
     * Starts and sets up an activity to render the provided site suggestions in the activity.
     * @return the layout in which the suggestions are rendered.
     */
    private TileGridLayout renderTiles(List<SiteSuggestion> siteSuggestions,
            List<String> offlineUrls) throws IOException, InterruptedException {
        // Launching the activity, that should now use the right UI.
        mActivityTestRule.startMainActivityOnBlankPage();
        ChromeActivity activity = mActivityTestRule.getActivity();

        // Setting up the dummy data.
        FakeMostVisitedSites mostVisitedSites = new FakeMostVisitedSites();
        mostVisitedSites.setTileSuggestions(siteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mostVisitedSites;
        mSuggestionsDeps.getFactory().suggestionsSource = new FakeSuggestionsSource();

        FrameLayout contentView = new FrameLayout(activity);
        UiConfig uiConfig = new UiConfig(contentView);

        return ThreadUtils.runOnUiThreadBlockingNoException(() -> {
            activity.setContentView(contentView);

            SiteSectionViewHolder viewHolder = SiteSection.createViewHolder(
                    SiteSection.inflateSiteSection(contentView), uiConfig);

            uiConfig.updateDisplayStyle();

            SiteSection siteSection = createSiteSection(viewHolder, uiConfig, offlineUrls);
            siteSection.getTileGroupForTesting().onSwitchToForeground(false);
            assertTrue("Tile Data should be visible.", siteSection.isVisible());

            siteSection.onBindViewHolder(viewHolder, 0);
            contentView.addView(viewHolder.itemView);

            return (TileGridLayout) viewHolder.itemView;
        });
    }

    private TileGridLayout renderTiles(List<SiteSuggestion> siteSuggestions)
            throws IOException, InterruptedException {
        return renderTiles(siteSuggestions, Collections.emptyList());
    }

    private SiteSection createSiteSection(
            final SiteSectionViewHolder viewHolder, UiConfig uiConfig, List<String> offlineUrls) {
        ThreadUtils.assertOnUiThread();

        ChromeActivity activity = mActivityTestRule.getActivity();

        Profile profile = Profile.getLastUsedProfile();
        SuggestionsUiDelegate uiDelegate = new SuggestionsUiDelegateImpl(
                mSuggestionsDeps.getFactory().createSuggestionSource(null),
                mSuggestionsDeps.getFactory().createEventReporter(), null, profile, null,
                activity.getChromeApplication().getReferencePool(), activity.getSnackbarManager());

        FakeOfflinePageBridge offlinePageBridge = new FakeOfflinePageBridge();
        List<OfflinePageItem> offlinePageItems = new ArrayList<>();
        for (int i = 0; i < offlineUrls.size(); i++) {
            offlinePageItems.add(
                    FakeOfflinePageBridge.createOfflinePageItem(offlineUrls.get(i), i + 1L));
        }
        offlinePageBridge.setItems(offlinePageItems);
        offlinePageBridge.setIsOfflinePageModelLoaded(true);

        TileGroup.Delegate delegate = new TileGroupDelegateImpl(activity, profile, null, null) {
            @Override
            public void onLoadingComplete(List<Tile> tiles) {
                super.onLoadingComplete(tiles);
                mLoadCompleteHelper.notifyCalled();
            }
        };

        SiteSection siteSection =
                new SiteSection(uiDelegate, null, delegate, offlinePageBridge, uiConfig);

        siteSection.addObserver(new ListObservable.ListObserver<PartialBindCallback>() {
            @Override
            public void onItemRangeChanged(ListObservable child, int index, int count,
                    @Nullable PartialBindCallback payload) {
                if (payload != null) payload.onResult(viewHolder);
            }
        });

        return siteSection;
    }
}
