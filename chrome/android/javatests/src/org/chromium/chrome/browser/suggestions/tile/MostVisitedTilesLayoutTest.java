// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import static android.content.res.Configuration.ORIENTATION_LANDSCAPE;
import static android.content.res.Configuration.ORIENTATION_PORTRAIT;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites.createSiteSuggestion;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.AfterClass;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegate;
import org.chromium.chrome.browser.suggestions.SuggestionsUiDelegateImpl;
import org.chromium.chrome.browser.ui.native_page.TouchEnabledDelegate;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.MvtsFacility;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.offlinepages.FakeOfflinePageBridge;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Instrumentation tests for the {@link MostVisitedTilesLayout} on the New Tab Page. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
@Batch(Batch.PER_CLASS)
public class MostVisitedTilesLayoutTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Rule public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(
                            ChromeRenderTestRule.Component.UI_BROWSER_CONTENT_SUGGESTIONS_HISTORY)
                    .build();

    @Mock ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock TouchEnabledDelegate mTouchEnabledDelegate;

    private static final String[] FAKE_MOST_VISITED_URLS =
            new String[] {
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

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @ParameterAnnotations.UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "RenderTest"})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    @DisableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
    public void testTilesLayoutAppearance_DisableMvtCustomization(boolean nightModeEnabled)
            throws Exception {
        doTilesLayoutAppearanceTest(nightModeEnabled, "");
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "RenderTest"})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    @EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
    public void testTilesLayoutAppearance_EnableMvtCustomization(boolean nightModeEnabled)
            throws Exception {
        doTilesLayoutAppearanceTest(nightModeEnabled, "_with_add_new_button");
    }

    private void doTilesLayoutAppearanceTest(boolean nightModeEnabled, String suffix)
            throws Exception {
        List<SiteSuggestion> siteSuggestions =
                makeAndSetUpFakeSuggestions(FAKE_MOST_VISITED_URLS.length);

        MvtsFacility mvts = mActivityTestRule.startOnNtp().focusOnMvts(siteSuggestions);
        mRenderTestRule.render(mvts.tilesLayoutElement.value(), "ntp_tile_layout" + suffix);
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "RenderTest"})
    @DisableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
    public void testModernTilesLayoutAppearance_Full_DisableMvtCustomization()
            throws IOException, InterruptedException {
        doModernTilesLayoutAppearanceTest_Full("");
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "RenderTest"})
    @EnableFeatures({ChromeFeatureList.MOST_VISITED_TILES_CUSTOMIZATION})
    public void testModernTilesLayoutAppearance_Full_EnableMvtCustomization()
            throws IOException, InterruptedException {
        doModernTilesLayoutAppearanceTest_Full("_with_add_new_button");
    }

    private void doModernTilesLayoutAppearanceTest_Full(String suffix)
            throws IOException, InterruptedException {
        makeAndSetUpFakeSuggestions(FAKE_MOST_VISITED_URLS.length);
        View tilesLayout = renderTiles();

        Activity activity = mActivityTestRule.getActivity();
        activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getResources().getConfiguration().orientation,
                            is(ORIENTATION_PORTRAIT));
                });
        mRenderTestRule.render(tilesLayout, "modern_tiles_layout_full_portrait" + suffix);

        activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getResources().getConfiguration().orientation,
                            is(ORIENTATION_LANDSCAPE));
                });
        mRenderTestRule.render(tilesLayout, "modern_tiles_layout_full_landscape" + suffix);

        // Reset device orientation.
        ActivityTestUtils.clearActivityOrientation(activity);
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "RenderTest"})
    public void testModernTilesLayoutAppearance_Two() throws IOException, InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(
                                /* nightModeEnabled= */ false));

        makeAndSetUpFakeSuggestions(2);
        View tilesLayout = renderTiles();

        Activity activity = mActivityTestRule.getActivity();
        activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getResources().getConfiguration().orientation,
                            is(ORIENTATION_PORTRAIT));
                });
        mRenderTestRule.render(tilesLayout, "modern_tiles_layout_two_tiles_portrait");

        activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            activity.getResources().getConfiguration().orientation,
                            is(ORIENTATION_LANDSCAPE));
                });
        mRenderTestRule.render(tilesLayout, "modern_tiles_layout_two_tiles_landscape");

        // Reset device orientation.
        ActivityTestUtils.clearActivityOrientation(activity);
    }

    @Test
    @MediumTest
    @Feature({"NewTabPage", "RenderTest"})
    @ParameterAnnotations.UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testTileAppearanceModern(boolean nightModeEnabled)
            throws IOException, InterruptedException, TimeoutException {
        List<SiteSuggestion> suggestions = makeAndSetUpFakeSuggestions(2);
        List<GURL> offlineAvailableUrls = Collections.singletonList(suggestions.get(0).url);
        ViewGroup tiles = renderTiles(offlineAvailableUrls);

        mLoadCompleteHelper.waitForCallback(0);

        mRenderTestRule.render(tiles.getChildAt(0), "tile_modern_offline");
        mRenderTestRule.render(tiles.getChildAt(1), "tile_modern");
    }

    private List<SiteSuggestion> makeAndSetUpFakeSuggestions(int count) {
        List<SiteSuggestion> siteSuggestions = makeFakeSuggestions(count);

        FakeMostVisitedSites mMostVisitedSites = new FakeMostVisitedSites();
        mMostVisitedSites.setTileSuggestions(siteSuggestions);
        mSuggestionsDeps.getFactory().mostVisitedSites = mMostVisitedSites;

        return siteSuggestions;
    }

    private List<SiteSuggestion> makeFakeSuggestions(int count) {
        List<SiteSuggestion> siteSuggestions = new ArrayList<>(count);

        assertEquals(FAKE_MOST_VISITED_URLS.length, FAKE_MOST_VISITED_TITLES.length);
        assertTrue(count <= FAKE_MOST_VISITED_URLS.length);

        for (int i = 0; i < count; i++) {
            String url = mTestServerRule.getServer().getURL(FAKE_MOST_VISITED_URLS[i]);
            siteSuggestions.add(createSiteSuggestion(FAKE_MOST_VISITED_TITLES[i], url));
        }
        return siteSuggestions;
    }

    /**
     * Starts and sets up an activity to render the provided site suggestions in the activity.
     *
     * @return the layout in which the suggestions are rendered.
     */
    private ViewGroup renderTiles(List<GURL> offlineUrls) throws InterruptedException {
        // Launching the activity, that should now use the right UI.
        mActivityTestRule.startOnBlankPage();
        ChromeActivity activity = mActivityTestRule.getActivity();

        ViewGroup contentView = new FrameLayout(activity);

        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    setOfflinePageBridge(offlineUrls);
                    activity.setContentView(contentView);
                    ViewGroup containerLayout =
                            (ViewGroup)
                                    LayoutInflater.from(contentView.getContext())
                                            .inflate(R.layout.mv_tiles_layout, contentView, false);
                    containerLayout.setVisibility(View.VISIBLE);
                    contentView.addView(containerLayout);
                    initializeCoordinator(containerLayout);
                    TilesLinearLayout mvTilesLayout =
                            containerLayout.findViewById(R.id.mv_tiles_layout);
                    assertNotNull(mvTilesLayout);
                    return mvTilesLayout;
                });
    }

    private ViewGroup renderTiles() throws InterruptedException {
        return renderTiles(Collections.emptyList());
    }

    private void setOfflinePageBridge(List<GURL> offlineUrls) {
        FakeOfflinePageBridge offlinePageBridge = new FakeOfflinePageBridge();
        List<OfflinePageItem> offlinePageItems = new ArrayList<>();
        for (int i = 0; i < offlineUrls.size(); i++) {
            offlinePageItems.add(
                    FakeOfflinePageBridge.createOfflinePageItem(
                            offlineUrls.get(i).getSpec(), i + 1L));
        }
        offlinePageBridge.setItems(offlinePageItems);
        offlinePageBridge.setIsOfflinePageModelLoaded(true);
        mSuggestionsDeps.getFactory().offlinePageBridge = offlinePageBridge;
    }

    private void initializeCoordinator(ViewGroup containerLayout) {
        ThreadUtils.assertOnUiThread();

        ChromeActivity activity = mActivityTestRule.getActivity();

        // TODO (https://crbug.com/1063807):  Add incognito mode tests.
        Profile profile = ProfileManager.getLastUsedRegularProfile();
        SuggestionsUiDelegate uiDelegate =
                new SuggestionsUiDelegateImpl(null, profile, null, activity.getSnackbarManager());

        TileGroup.Delegate delegate =
                new TileGroupDelegateImpl(activity, profile, null, null) {
                    @Override
                    public void onLoadingComplete(List<Tile> tiles) {
                        super.onLoadingComplete(tiles);
                        mLoadCompleteHelper.notifyCalled();
                    }
                };

        MostVisitedTilesCoordinator coordinator =
                new MostVisitedTilesCoordinator(
                        activity, mActivityLifecycleDispatcher, containerLayout, null, null);
        coordinator.initWithNative(profile, uiDelegate, delegate, mTouchEnabledDelegate);
    }
}
