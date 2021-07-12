// Copyright 2021 The Chromium Authors. All rights reserved.
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
import static org.junit.Assert.fail;

import static org.chromium.chrome.browser.tabmodel.TestTabModelDirectory.M26_GOOGLE_COM;
import static org.chromium.chrome.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Bitmap;
import android.support.test.InstrumentationRegistry;
import android.util.Base64;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.matcher.ViewMatchers;

import org.hamcrest.Matcher;
import org.junit.Assert;

import org.chromium.base.StreamUtil;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.suggestions.SiteSuggestion;
import org.chromium.chrome.browser.suggestions.tile.TileSectionType;
import org.chromium.chrome.browser.suggestions.tile.TileSource;
import org.chromium.chrome.browser.suggestions.tile.TileTitleSource;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.TabStateFileManager;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;
import org.chromium.chrome.browser.tabpersistence.TabStateDirectory;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.start_surface.R;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.chrome.test.util.browser.suggestions.mostvisited.FakeMostVisitedSites;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.url.GURL;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Utility methods and classes for testing Start Surface.
 */
public class StartSurfaceTestUtils {
    private static final long MAX_TIMEOUT_MS = 30000L;

    /**
     * Only launch Chrome without waiting for a current tab.
     * This method could not use {@link ChromeActivityTestRule#startMainActivityFromLauncher()}
     * because of its {@link org.chromium.chrome.browser.tab.Tab} dependency.
     */
    public static void startMainActivityFromLauncher(ChromeActivityTestRule activityTestRule) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        activityTestRule.prepareUrlIntent(intent, null);
        activityTestRule.launchActivity(intent);
    }

    /**
     * Wait for the start surface homepage or tab switcher page visible.
     * @param cta The ChromeTabbedActivity under test.
     */
    public static void waitForOverviewVisible(ChromeTabbedActivity cta) {
        CriteriaHelper.pollUiThread(()
                                            -> cta.getLayoutManager() != null
                        && cta.getLayoutManager().overviewVisible(),
                MAX_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Wait for the start surface homepage or tab switcher page visible.
     * @param layoutChangedCallbackHelper The call back function to help check whether layout is
     *         changed.
     * @param currentlyActiveLayout The current active layout.
     */
    public static void waitForOverviewVisible(
            CallbackHelper layoutChangedCallbackHelper, @LayoutType int currentlyActiveLayout) {
        if (currentlyActiveLayout == LayoutType.TAB_SWITCHER) return;
        try {
            layoutChangedCallbackHelper.waitForNext(30L, TimeUnit.SECONDS);
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
     */
    public static void createTabStateFile(int[] tabIds, @Nullable String[] urls)
            throws IOException {
        createTabStateFile(tabIds, urls, 0);
    }

    /**
     * Create all the files so that tab models can be restored.
     * @param tabIds all the Tab IDs in the normal tab model.
     * @param urls all of the URLs in the normal tab model.
     * @param selectedIndex the selected index of normal tab model.
     */
    public static void createTabStateFile(int[] tabIds, @Nullable String[] urls, int selectedIndex)
            throws IOException {
        TabPersistentStore.TabModelMetadata normalInfo =
                new TabPersistentStore.TabModelMetadata(selectedIndex);
        for (int i = 0; i < tabIds.length; i++) {
            normalInfo.ids.add(tabIds[i]);
            String url = urls != null ? urls[i] : "about:blank";
            normalInfo.urls.add(url);

            saveTabState(tabIds[i], false);
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
     * Create thumbnail bitmap of the tab based on the given id and write it to file.
     * @param tabId The id of the target tab.
     * @return The bitmap created.
     */
    public static Bitmap createThumbnailBitmapAndWriteToFile(int tabId) {
        final Bitmap thumbnailBitmap = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);

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
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.INVISIBLE)));
        // The home button shouldn't show on homepage.
        onView(withId(R.id.home_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));

        // Drag the Feed header title to scroll the toolbar to the top.
        int toY = -cta.getResources().getDimensionPixelOffset(R.dimen.toolbar_height_no_shadow);
        TestTouchUtils.dragCompleteView(InstrumentationRegistry.getInstrumentation(),
                cta.findViewById(R.id.header_title), 0, 0, 0, toY, 1);

        // The start surface toolbar should be scrolled up and not be displayed.
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> cta.findViewById(R.id.tab_switcher_toolbar).getTranslationY()
                        <= (float) -cta.getResources().getDimensionPixelOffset(
                                R.dimen.toolbar_height_no_shadow));

        // Toolbar layout view should show.
        onViewWaiting(withId(R.id.toolbar));
        // The home button shouldn't show on homepage whether it's scrolled or not.
        onView(withId(R.id.home_button))
                .check(matches(withEffectiveVisibility(ViewMatchers.Visibility.GONE)));

        // The start surface toolbar should be scrolled up and not be displayed.
        onView(withId(R.id.tab_switcher_toolbar)).check(matches(not(isDisplayed())));

        // Check the toolbar's background color.
        ToolbarPhone toolbar = cta.findViewById(org.chromium.chrome.R.id.toolbar);
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
     * Click the first MV tile (Explore tile) in mv_tiles_layout.
     * @param cta The ChromeTabbedActivity under test.
     * @param currentTabCount The correct number of normal tabs.
     */
    public static void launchFirstMVTile(ChromeTabbedActivity cta, int currentTabCount) {
        TabUiTestHelper.verifyTabModelTabCount(cta, currentTabCount, 0);
        OverviewModeBehaviorWatcher hideWatcher = TabUiTestHelper.createOverviewHideWatcher(cta);
        onViewWaiting(withId(org.chromium.chrome.tab_ui.R.id.mv_tiles_layout))
                .perform(new ViewAction() {
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
        hideWatcher.waitForBehavior();
        CriteriaHelper.pollUiThread(() -> !cta.getLayoutManager().overviewVisible());
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
        onViewWaiting(
                allOf(withParent(withId(
                              org.chromium.chrome.tab_ui.R.id.carousel_tab_switcher_container)),
                        withId(org.chromium.chrome.tab_ui.R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(position, click()));
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
                    ()
                            -> cta.findViewById(org.chromium.chrome.tab_ui.R.id.more_tabs)
                                       .performClick());
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
        siteSuggestions.add(new SiteSuggestion("0 EXPLORE_SITES", new GURL("https://www.bar.com"),
                "", TileTitleSource.UNKNOWN, TileSource.EXPLORE, TileSectionType.PERSONALIZED,
                new Date()));

        for (int i = 0; i < 7; i++) {
            siteSuggestions.add(new SiteSuggestion(String.valueOf(i),
                    new GURL("https://www." + i + ".com"), "", TileTitleSource.TITLE_TAG,
                    TileSource.TOP_SITES, TileSectionType.PERSONALIZED, new Date()));
        }

        return siteSuggestions;
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
