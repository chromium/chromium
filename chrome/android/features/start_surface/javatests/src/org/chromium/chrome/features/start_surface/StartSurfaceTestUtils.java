// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import static org.chromium.chrome.browser.tabmodel.TestTabModelDirectory.M26_GOOGLE_COM;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.util.Base64;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.uiautomator.UiDevice;

import org.hamcrest.Matcher;
import org.junit.Assert;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.NativeLibraryLoadedStatus;
import org.chromium.base.StreamUtil;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.gesturenav.GestureNavigationUtils;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.TileSectionType;
import org.chromium.chrome.browser.suggestions.tile.TileSource;
import org.chromium.chrome.browser.suggestions.tile.TileTitleSource;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tabpersistence.TabStateFileManager;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Utility methods and classes for testing Start Surface.
 */
public class StartSurfaceTestUtils {
    public static final String INSTANT_START_TEST_BASE_PARAMS =
            "force-fieldtrial-params=Study.Group:"
            + StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS_PARAM + "/0";
    public static final String START_SURFACE_TEST_SINGLE_ENABLED_PARAMS =
            "force-fieldtrial-params=Study.Group:show_last_active_tab_only/false"
            + "/open_ntp_instead_of_start/false/open_start_as_homepage/true";
    public static final String START_SURFACE_TEST_BASE_PARAMS =
            "force-fieldtrial-params=Study.Group:";

    public static final String START_SURFACE_ON_TABLET_TEST_PARAMS =
            "force-fieldtrial-params=Study.Group:"
            + StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_ON_TABLET_SECONDS_PARAM + "/0";
    public static List<ParameterSet> sClassParamsForStartSurfaceTest =
            Arrays.asList(new ParameterSet().value(false, false).name("NoInstant_NoReturn"),
                    new ParameterSet().value(true, false).name("Instant_NoReturn"),
                    new ParameterSet().value(false, true).name("NoInstant_Return"),
                    new ParameterSet().value(true, true).name("Instant_Return"));

    /**
     * {@link ParameterProvider} used for parameterized tests with/without "Start Surface refactor"
     * flag enabled.
     */
    public static class RefactorTestParams implements ParameterProvider {
        private static List<ParameterSet> sRefactorTestParams =
                Arrays.asList(new ParameterSet().value(false).name("RefactorDisabled"),
                        new ParameterSet().value(true).name("RefactorEnabled"));

        @Override
        public List<ParameterSet> getParameters() {
            return sRefactorTestParams;
        }
    }

    /**
     * {@link ParameterProvider} used for tests with "Start Surface refactor" flag disabled.
     */
    public static class LegacyTestParams implements ParameterProvider {
        private static List<ParameterSet> sLegacyTestParams =
                Arrays.asList(new ParameterSet().value(false));

        @Override
        public List<ParameterSet> getParameters() {
            return sLegacyTestParams;
        }
    }

    private static final long MAX_TIMEOUT_MS = 30000L;

    /**
     * Set up StartSurfaceTest* based on whether it's immediateReturn or not.
     * @param immediateReturn Whether feature {@link ChromeFeatureList#START_SURFACE_RETURN_TIME} is
     *                        enabled as "immediately". When immediate return is enabled, the Start
     *                        surface is showing when Chrome is launched.
     * @param activityTestRule The test rule of activity under test.
     */
    public static void setUpStartSurfaceTests(boolean immediateReturn,
            ChromeTabbedActivityTestRule activityTestRule) throws IOException {
        BrowserControlsStateProvider fakeBrowserControlsStateProvider =
                new BrowserControlsStateProvider() {
                    @Override
                    public void addObserver(BrowserControlsStateProvider.Observer obs) {
                        assert false : "Not reached";
                    }

                    @Override
                    public void removeObserver(BrowserControlsStateProvider.Observer obs) {
                        assert false : "Not reached";
                    }

                    @Override
                    public int getTopControlsHeight() {
                        return 0;
                    }

                    @Override
                    public int getTopControlsMinHeight() {
                        return 0;
                    }

                    @Override
                    public int getTopControlOffset() {
                        return 0;
                    }

                    @Override
                    public int getTopControlsMinHeightOffset() {
                        return 0;
                    }

                    @Override
                    public int getBottomControlsHeight() {
                        return 0;
                    }

                    @Override
                    public int getBottomControlsMinHeight() {
                        return 0;
                    }

                    @Override
                    public int getBottomControlsMinHeightOffset() {
                        return 0;
                    }

                    @Override
                    public boolean shouldAnimateBrowserControlsHeightChanges() {
                        return false;
                    }

                    @Override
                    public int getBottomControlOffset() {
                        return 0;
                    }

                    @Override
                    public float getBrowserControlHiddenRatio() {
                        return 0.0f;
                    }

                    @Override
                    public int getContentOffset() {
                        return 0;
                    }

                    @Override
                    public float getTopVisibleContentOffset() {
                        return 0.0f;
                    }

                    @Override
                    public int getAndroidControlsVisibility() {
                        return 0;
                    }
                };

        int expectedTabs = 1;
        int additionalTabs = expectedTabs - (immediateReturn ? 0 : 1);
        if (additionalTabs > 0) {
            int[] tabIDs = new int[additionalTabs];
            for (int i = 0; i < additionalTabs; i++) {
                tabIDs[i] = i;
                createThumbnailBitmapAndWriteToFile(i, fakeBrowserControlsStateProvider);
            }
            createTabStateFile(tabIDs);
        }
        if (immediateReturn) {
            StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS.setForTesting(0);
            assertEquals(0, StartSurfaceConfiguration.START_SURFACE_RETURN_TIME_SECONDS.getValue());
            assertTrue(ReturnToChromeUtil.shouldShowTabSwitcher(-1, false));

            // Need to start main activity from launcher for immediate return to be effective.
            // However, need at least one tab for carousel to show, which starting main activity
            // from launcher doesn't provide.
            // Creating tabs and restart the activity would do the trick, but we cannot do that for
            // Instant start because we cannot unload native library.
            // Create fake TabState files to emulate having one tab in previous session.
            TabAttributeCache.setTitleForTesting(0, "tab title");
            startMainActivityFromLauncher(activityTestRule);
        } else {
            assertFalse(ReturnToChromeUtil.shouldShowTabSwitcher(-1, false));
            // Cannot use StartSurfaceTestUtils.startMainActivityFromLauncher().
            // Otherwise tab switcher could be shown immediately if single-pane is enabled.
            activityTestRule.startMainActivityOnBlankPage();
            onViewWaiting(withId(R.id.home_button));
        }
    }

    /**
     * Only launch Chrome without waiting for a current tab.
     * This method could not use {@link
     * ChromeTabbedActivityTestRule#startMainActivityFromLauncher()} because of its {@link
     * org.chromium.chrome.browser.tab.Tab} dependency.
     */
    public static void startMainActivityFromLauncher(ChromeActivityTestRule activityTestRule) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        activityTestRule.prepareUrlIntent(intent, null);
        activityTestRule.launchActivity(intent);
    }

    public static void startAndWaitNativeInitialization(
            ChromeTabbedActivityTestRule activityTestRule) {
        Assert.assertTrue(NativeLibraryLoadedStatus.getProviderForTesting() == null
                || !NativeLibraryLoadedStatus.getProviderForTesting().areNativeMethodsReady());

        CommandLine.getInstance().removeSwitch(ChromeSwitches.DISABLE_NATIVE_INITIALIZATION);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> activityTestRule.getActivity().startDelayedNativeInitializationForTests());
        CriteriaHelper.pollUiThread(
                activityTestRule.getActivity().getTabModelSelector()::isTabStateInitialized, 10000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        Assert.assertTrue(LibraryLoader.getInstance().isInitialized());
        ChromeTabbedActivity cta = activityTestRule.getActivity();
        StartSurfaceTestUtils.waitForStartSurfaceVisible(cta);
    }

    /**
     * Wait for the start surface homepage visible.
     * @param cta The ChromeTabbedActivity under test.
     */
    public static void waitForStartSurfaceVisible(ChromeTabbedActivity cta) {
        CriteriaHelper.pollUiThread(() -> cta.getLayoutManager() != null);
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), getStartSurfaceLayoutType());

        onViewWaiting(allOf(withId(R.id.primary_tasks_surface_view), isDisplayed()));
    }

    /**
     * Wait for the tab switcher page visible.
     * @param cta The ChromeTabbedActivity under test.
     */
    public static void waitForTabSwitcherVisible(ChromeTabbedActivity cta) {
        if (ChromeFeatureList.sStartSurfaceRefactor.isEnabled()) {
            LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);
        } else {
            // TODO(1315676): Removes here when the Start surface refactoring is enabled by default.
            onViewWaiting(withId(R.id.secondary_tasks_surface_view));
        }
    }

    public static @LayoutType int getStartSurfaceLayoutType() {
        return ChromeFeatureList.sStartSurfaceRefactor.isEnabled() ? LayoutType.START_SURFACE
                                                                   : LayoutType.TAB_SWITCHER;
    }

    /**
     * Wait for the start surface homepage visible.
     * @param layoutChangedCallbackHelper The call back function to help check whether layout is
     *         changed.
     * @param currentlyActiveLayout The current active layout.
     * @param cta The ChromeTabbedActivity under test.
     */
    public static void waitForStartSurfaceVisible(CallbackHelper layoutChangedCallbackHelper,
            @LayoutType int currentlyActiveLayout, ChromeTabbedActivity cta) {
        waitForLayoutVisible(layoutChangedCallbackHelper, currentlyActiveLayout, cta,
                getStartSurfaceLayoutType());

        onViewWaiting(allOf(withId(R.id.primary_tasks_surface_view), isDisplayed()));
    }

    /**
     * Wait for the tab switcher page visible.
     * @param layoutChangedCallbackHelper The call back function to help check whether layout is
     *         changed.
     * @param currentlyActiveLayout The current active layout.
     * @param cta The ChromeTabbedActivity under test.
     */
    public static void waitForTabSwitcherVisible(CallbackHelper layoutChangedCallbackHelper,
            @LayoutType int currentlyActiveLayout, ChromeTabbedActivity cta) {
        waitForLayoutVisible(
                layoutChangedCallbackHelper, currentlyActiveLayout, cta, LayoutType.TAB_SWITCHER);
    }

    private static void waitForLayoutVisible(CallbackHelper layoutChangedCallbackHelper,
            @LayoutType int currentlyActiveLayout, ChromeTabbedActivity cta,
            @LayoutType int layoutType) {
        if (currentlyActiveLayout == layoutType) {
            StartSurfaceTestUtils.waitForTabModel(cta);
            return;
        }
        try {
            layoutChangedCallbackHelper.waitForNext(30L, TimeUnit.SECONDS);
            StartSurfaceTestUtils.waitForTabModel(cta);
        } catch (TimeoutException ex) {
            assert false : "Timeout waiting for browser to enter tab switcher / start surface.";
        }
    }

    /**
     * Wait for the tab state to be initialized.
     * @param cta The ChromeTabbedActivity under test.
     */
    public static void waitForTabModel(ChromeTabbedActivity cta) {
        CriteriaHelper.pollUiThread(cta.getTabModelSelector()::isTabStateInitialized,
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Create all the files so that tab models can be restored.
     * @param tabIds all the Tab IDs in the normal tab model.
     */
    public static void createTabStateFile(int[] tabIds) throws IOException {
        createTabStateFile(tabIds, null, 0);
    }

    /**
     * Create all the files so that tab models can be restored.
     * @param tabIds all the Tab IDs in the normal tab model.
     * @param urls all of the URLs in the normal tab model.
     * @param selectedIndex the selected index of normal tab model.
     */
    public static void createTabStateFile(int[] tabIds, @Nullable String[] urls, int selectedIndex)
            throws IOException {
        createTabStateFile(tabIds, urls, selectedIndex, true);
    }

    private static void createTabStateFile(int[] tabIds, @Nullable String[] urls, int selectedIndex,
            boolean createStateFile) throws IOException {
        TabPersistentStore.TabModelMetadata normalInfo =
                new TabPersistentStore.TabModelMetadata(selectedIndex);
        for (int i = 0; i < tabIds.length; i++) {
            normalInfo.ids.add(tabIds[i]);
            String url = urls != null ? urls[i] : "about:blank";
            normalInfo.urls.add(url);

            if (createStateFile) {
                saveTabState(tabIds[i], false);
            }
        }
        TabPersistentStore.TabModelMetadata incognitoInfo =
                new TabPersistentStore.TabModelMetadata(0);

        byte[] listData = TabPersistentStore.serializeMetadata(normalInfo, incognitoInfo);

        File stateFile = new File(TabStateDirectory.getOrCreateTabbedModeStateDirectory(),
                TabbedModeTabPersistencePolicy.getStateFileName(0));
        FileOutputStream output = new FileOutputStream(stateFile);
        output.write(listData);
        output.close();
    }

    /**
     * Creates a Tab state metadata file without creating Tab state files for the given Tab's info.
     * @param tabIds All the Tab IDs in the normal tab model.
     * @param urls All the Tab URLs in the normal tab model.
     * @param selectedIndex The selected index of normal tab model.
     */
    public static void prepareTabStateMetadataFile(
            int[] tabIds, @Nullable String[] urls, int selectedIndex) throws IOException {
        createTabStateFile(tabIds, urls, selectedIndex, false);
    }

    /**
     * Create thumbnail bitmap of the tab based on the given id and write it to file.
     * @param tabId The id of the target tab.
     * @param browserControlsStateProvider For getting the top offset.
     * @return The bitmap created.
     */
    public static Bitmap createThumbnailBitmapAndWriteToFile(
            int tabId, BrowserControlsStateProvider browserControlsStateProvider) {
        final int height = 100;
        final int width = (int) Math.round(height
                * TabUtils.getTabThumbnailAspectRatio(
                        ContextUtils.getApplicationContext(), browserControlsStateProvider));
        final Bitmap thumbnailBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);

        try {
            File thumbnailFile = TabContentManager.getTabThumbnailFileJpeg(tabId);
            if (thumbnailFile.exists()) {
                thumbnailFile.delete();
            }
            Assert.assertFalse(thumbnailFile.exists());

            FileOutputStream thumbnailFileOutputStream = new FileOutputStream(thumbnailFile);
            thumbnailBitmap.compress(Bitmap.CompressFormat.JPEG, 100, thumbnailFileOutputStream);
            thumbnailFileOutputStream.flush();
            thumbnailFileOutputStream.close();

            Assert.assertTrue(thumbnailFile.exists());
        } catch (IOException e) {
            e.printStackTrace();
        }
        return thumbnailBitmap;
    }

    /**
     * Get the StartSurfaceCoordinator from UI thread.
     * @param cta The ChromeTabbedActivity under test.
     */
    public static StartSurfaceCoordinator getStartSurfaceFromUIThread(ChromeTabbedActivity cta) {
        AtomicReference<StartSurface> startSurface = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> startSurface.set(cta.getStartSurface()));
        return (StartSurfaceCoordinator) startSurface.get();
    }

    /**
     * Gets the current active Tab from UI thread.
     * @param cta The ChromeTabbedActivity under test.
     */
    public static Tab getCurrentTabFromUIThread(ChromeTabbedActivity cta) {
        AtomicReference<Tab> tab = new AtomicReference<>();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> tab.set(TabModelUtils.getCurrentTab(cta.getCurrentTabModel())));
        return tab.get();
    }

    /**
     * @param activityTestRule The test rule of activity under test.
     * @return Whether the keyboard is shown.
     */
    public static boolean isKeyboardShown(ChromeActivityTestRule activityTestRule) {
        Activity activity = activityTestRule.getActivity();
        if (activity.getCurrentFocus() == null) return false;
        return activityTestRule.getKeyboardDelegate().isKeyboardShowing(
                activity, activity.getCurrentFocus());
    }

    /**
     * Scroll the start surface to make toolbar scrolled off.
     * @param cta The ChromeTabbedActivity under test.
     */
    public static void scrollToolbar(ChromeTabbedActivity cta) {
        // Toolbar layout should be hidden if start surface toolbar is shown on the top of the
        // screen.
        onView(withId(R.id.toolbar))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
        // The home button shouldn't show on homepage.
        onView(withId(R.id.home_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));

        // Drag the Feed header title to scroll the toolbar to the top.
        int toY = -cta.getResources().getDimensionPixelOffset(R.dimen.toolbar_height_no_shadow);
        TestTouchUtils.dragCompleteView(InstrumentationRegistry.getInstrumentation(),
                cta.findViewById(R.id.header_title), 0, 0, 0, toY, 10);

        // The start surface toolbar should be scrolled up and not be displayed.
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> cta.findViewById(R.id.tab_switcher_toolbar).getTranslationY()
                        <= (float) -cta.getResources().getDimensionPixelOffset(
                                R.dimen.toolbar_height_no_shadow));

        // Toolbar layout view should show.
        onViewWaiting(withId(R.id.toolbar));

        // The start surface toolbar should be scrolled up and not be displayed.
        onView(withId(R.id.tab_switcher_toolbar)).check(matches(not(isDisplayed())));

        // Check the toolbar's background color.
        ToolbarPhone toolbar = cta.findViewById(R.id.toolbar);
        Assert.assertEquals(toolbar.getToolbarDataProvider().getPrimaryColor(),
                toolbar.getBackgroundDrawable().getColor());
    }

    /**
     * Navigate to homepage.
     * @param cta The ChromeTabbedActivity under test.
     */
    public static void pressHomePageButton(ChromeTabbedActivity cta) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            cta.getToolbarManager().getToolbarTabControllerForTesting().openHomepage();
        });
    }

    /**
     * Simulate pressing the back button.
     * @param activityTestRule The ChromeTabbedActivityTestRule under test.
     */
    public static void pressBack(ChromeTabbedActivityTestRule activityTestRule) {
        // ChromeTabbedActivity expects the native libraries to be loaded when back is pressed.
        activityTestRule.waitForActivityNativeInitializationComplete();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> activityTestRule.getActivity().getOnBackPressedDispatcher().onBackPressed());
    }

    /**
     * Perform gesture navigate back action.
     * @param activityTestRule The ChromeTabbedActivityTestRule under test.
     */
    public static void gestureNavigateBack(ChromeTabbedActivityTestRule activityTestRule) {
        GestureNavigationUtils navUtils = new GestureNavigationUtils(activityTestRule);
        navUtils.swipeFromLeftEdge();
    }

    /**
     * Click the first MV tile (Explore tile) in mv_tiles_layout.
     * @param cta The ChromeTabbedActivity under test.
     * @param currentTabCount The correct number of normal tabs.
     */
    public static void launchFirstMVTile(ChromeTabbedActivity cta, int currentTabCount) {
        TabUiTestHelper.verifyTabModelTabCount(cta, currentTabCount, 0);
        onViewWaiting(withId(R.id.mv_tiles_layout)).perform(new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return isDisplayed();
            }

            @Override
            public String getDescription() {
                return "Click explore top sites view in MV tiles.";
            }

            @Override
            public void perform(UiController uiController, View view) {
                ViewGroup mvTilesContainer = (ViewGroup) view;
                mvTilesContainer.getChildAt(0).performClick();
            }
        });
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
        // Verifies a new Tab is created.
        TabUiTestHelper.verifyTabModelTabCount(cta, currentTabCount + 1, 0);
    }

    /**
     * Click the first tab in carousel tab switcher.
     */
    public static void clickFirstTabInCarousel() {
        clickTabInCarousel(0);
    }

    /**
     * Click the tab at specific position in carousel tab switcher.
     * @param position The position of the tab which is clicked.
     */
    public static void clickTabInCarousel(int position) {
        onViewWaiting(allOf(withParent(withId(R.id.tab_switcher_module_container)),
                              withId(R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(position, click()));
    }

    /**
     * Click the tab switcher button to navigate to tab switcher surface.
     * @param cta The ChromeTabbedActivity under test.
     */
    public static void clickTabSwitcherButton(ChromeTabbedActivity cta) {
        try {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> cta.findViewById(R.id.start_tab_switcher_button).performClick());
        } catch (ExecutionException e) {
            fail("Failed to tap 'more tabs' " + e.toString());
        }
    }

    /**
     * Click "more_tabs" to navigate to tab switcher surface.
     * @param cta The ChromeTabbedActivity under test.
     */
    public static void clickMoreTabs(ChromeTabbedActivity cta) {
        // Note that onView(R.id.more_tabs).perform(click()) can not be used since it requires 90
        // percent of the view's area is displayed to the users. However, this view has negative
        // margin which makes the percentage is less than 90.
        // TODO(crbug.com/1186752): Investigate whether this would be a problem for real users.
        try {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> cta.findViewById(R.id.more_tabs).performClick());
        } catch (ExecutionException e) {
            fail("Failed to tap 'more tabs' " + e.toString());
        }
    }

    /**
     * Set MV tiles on start surface by setting suggestionsDeps.
     * @param suggestionsDeps The SuggestionsDependenciesRule under test.
     * @return The MostVisitedSites the test used.
     */
    public static FakeMostVisitedSites setMVTiles(SuggestionsDependenciesRule suggestionsDeps) {
        FakeMostVisitedSites mostVisitedSites = new FakeMostVisitedSites();
        mostVisitedSites.setTileSuggestions(createFakeSiteSuggestions());
        suggestionsDeps.getFactory().mostVisitedSites = mostVisitedSites;
        return mostVisitedSites;
    }

    /**
     * Returns a list of SiteSuggestion.
     */
    public static List<SiteSuggestion> createFakeSiteSuggestions() {
        List<SiteSuggestion> siteSuggestions = new ArrayList<>();
        String urlTemplate = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1_NUMERAL).serialize();
        for (int i = 0; i < 8; i++) {
            siteSuggestions.add(new SiteSuggestion(String.valueOf(i),
                    // Use pre-serialized GURL to avoid loading native.
                    GURL.deserialize(urlTemplate.replace("www.1.com", "www." + i + ".com")),
                    TileTitleSource.TITLE_TAG, TileSource.TOP_SITES, TileSectionType.PERSONALIZED));
        }

        return siteSuggestions;
    }

    /**
     * Wait for the deferred startup to be triggered.
     * @param activityTestRule The ChromeTabbedActivityTestRule under test.
     */
    public static void waitForDeferredStartup(ChromeTabbedActivityTestRule activityTestRule) {
        // Waits for the current Tab to complete loading. The deferred startup will be triggered
        // after the loading.
        waitForCurrentTabLoaded(activityTestRule);
        assertTrue("Deferred startup never completed", activityTestRule.waitForDeferredStartup());
    }

    /**
     * Waits for the current Tab to complete loading.
     * @param activityTestRule The ChromeTabbedActivityTestRule under test.
     */
    public static void waitForCurrentTabLoaded(ChromeTabbedActivityTestRule activityTestRule) {
        Tab tab = activityTestRule.getActivity().getActivityTab();
        if (tab != null && tab.isLoading()) {
            CriteriaHelper.pollUiThread(()
                                                -> !tab.isLoading(),
                    MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        }
    }

    /**
     * Simulates pressing the Android's home button and bringing Chrome to the background.
     */
    public static void pressHome() {
        UiDevice device = UiDevice.getInstance(InstrumentationRegistry.getInstrumentation());
        device.pressHome();
        ChromeApplicationTestUtils.waitUntilChromeInBackground();
    }

    /**
     * Gets the "tab_list_view" from the carousel tab switcher module on Start surface.
     */
    static View getCarouselTabSwitcherTabListView(ChromeTabbedActivity cta) {
        return cta.findViewById(R.id.tab_switcher_module_container)
                .findViewById(R.id.tab_list_view);
    }

    /**
     * Presses the back button and verifies that Chrome goes to the background.
     */
    public static void pressBackAndVerifyChromeToBackground(ChromeTabbedActivityTestRule testRule) {
        // Verifies Chrome is closed.
        AsyncInitializationActivity.interceptMoveTaskToBackForTesting();
        pressBack(testRule);
        Assert.assertTrue(AsyncInitializationActivity.wasMoveTaskToBackInterceptedForTesting());
    }

    /**
     * Create a file so that a TabState can be restored later.
     * @param tabId the Tab ID
     * @param encrypted for Incognito mode
     */
    private static void saveTabState(int tabId, boolean encrypted) {
        File file = TabStateFileManager.getTabStateFile(
                TabStateDirectory.getOrCreateTabbedModeStateDirectory(), tabId, encrypted);
        writeFile(file, M26_GOOGLE_COM.encodedTabState);

        TabState tabState = TabStateFileManager.restoreTabState(file, false);
        tabState.rootId = PseudoTab.fromTabId(tabId).getRootId();
        TabStateFileManager.saveState(file, tabState, encrypted);
    }

    private static void writeFile(File file, String data) {
        FileOutputStream outputStream = null;
        try {
            outputStream = new FileOutputStream(file);
            outputStream.write(Base64.decode(data, 0));
        } catch (Exception e) {
            assert false : "Failed to create " + file;
        } finally {
            StreamUtil.closeQuietly(outputStream);
        }
    }
}
