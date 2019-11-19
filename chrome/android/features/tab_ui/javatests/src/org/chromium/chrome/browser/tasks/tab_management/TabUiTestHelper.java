// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.CallbackHelper.WAIT_TIMEOUT_SECONDS;
import static org.chromium.content_public.browser.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.content_public.browser.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;

import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.os.Build;
import android.provider.Settings;
import android.support.annotation.Nullable;
import android.support.test.InstrumentationRegistry;
import android.support.test.espresso.NoMatchingViewException;
import android.support.test.espresso.ViewAssertion;
import android.support.test.espresso.contrib.RecyclerViewActions;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.OverviewModeBehaviorWatcher;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.File;
import java.util.List;

/**
 * Utilities helper class for tab grid/group tests.
 */
public class TabUiTestHelper {
    /**
     * Create {@code tabsCount} tabs for {@code cta} in certain tab model based on {@code
     * isIncognito}.
     * @param cta            A current running activity to create tabs.
     * @param isIncognito    Indicator for whether to create tabs in normal model or incognito
     *         model.
     * @param tabsCount      Number of tabs to be created.
     */
    static void createTabs(ChromeTabbedActivity cta, boolean isIncognito, int tabsCount) {
        for (int i = 0; i < (isIncognito ? tabsCount : tabsCount - 1); i++) {
            ChromeTabUtils.newTabFromMenu(
                    InstrumentationRegistry.getInstrumentation(), cta, isIncognito, true);
        }
    }

    /**
     * Enter tab switcher from a tab page.
     * @param cta  The current running activity.
     */
    public static void enterTabSwitcher(ChromeTabbedActivity cta) {
        OverviewModeBehaviorWatcher showWatcher = createOverviewShowWatcher(cta);
        assertFalse(cta.getLayoutManager().overviewVisible());
        // TODO(crbug.com/1019727) Figure out a better way to wait until isCompletelyDisplayed() is
        // true in espresso.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { cta.findViewById(R.id.tab_switcher_button).performClick(); });
        showWatcher.waitForBehavior();
    }

    /**
     * Click the first card in grid tab switcher. When group is enabled and the first card is a
     * group, this will open up the dialog; otherwise this will open up the tab page.
     * @param cta  The current running activity.
     */
    public static void clickFirstCardFromTabSwitcher(ChromeTabbedActivity cta) {
        clickNthCardFromTabSwitcher(cta, 0);
    }

    /**
     * Click the Nth card in grid tab switcher. When group is enabled and the Nth card is a
     * group, this will open up the dialog; otherwise this will open up the tab page.
     * @param cta  The current running activity.
     * @param index The index of the target card.
     */
    public static void clickNthCardFromTabSwitcher(ChromeTabbedActivity cta, int index) {
        assertTrue(cta.getLayoutManager().overviewVisible());
        onView(allOf(withParent(withId(org.chromium.chrome.R.id.compositor_view_holder)),
                       withId(R.id.tab_list_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(index, click()));
    }

    /**
     * Verify that current tab models hold correct number of tabs.
     * @param cta            The current running activity.
     * @param normalTabs     The correct number of normal tabs.
     * @param incognitoTabs  The correct number of incognito tabs.
     */
    static void verifyTabModelTabCount(
            ChromeTabbedActivity cta, int normalTabs, int incognitoTabs) {
        CriteriaHelper.pollUiThread(Criteria.equals(
                normalTabs, () -> cta.getTabModelSelector().getModel(false).getCount()));
        CriteriaHelper.pollUiThread(Criteria.equals(
                incognitoTabs, () -> cta.getTabModelSelector().getModel(true).getCount()));
    }

    /**
     * Verify there are correct number of cards in tab switcher.
     * @param cta       The current running activity.
     * @param count     The correct number of cards in tab switcher.
     */
    static void verifyTabSwitcherCardCount(ChromeTabbedActivity cta, int count) {
        assertTrue(cta.getLayoutManager().overviewVisible());
        onView(allOf(withParent(withId(org.chromium.chrome.R.id.compositor_view_holder)),
                       withId(R.id.tab_list_view)))
                .check(CardCountAssertion.havingTabCount(count));
    }

    /**
     * Create a tab group using {@code tabs}.
     * @param cta             The current running activity.
     * @param isIncognito     Whether the group is in normal model or incognito model.
     * @param tabs            A list of {@link Tab} to create group.
     */
    static void createTabGroup(ChromeTabbedActivity cta, boolean isIncognito, List<Tab> tabs) {
        if (tabs.size() == 0) return;
        assert cta.getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                        instanceof TabGroupModelFilter;
        TabGroupModelFilter filter = (TabGroupModelFilter) cta.getTabModelSelector()
                                             .getTabModelFilterProvider()
                                             .getTabModelFilter(isIncognito);
        Tab rootTab = tabs.get(0);
        for (int i = 1; i < tabs.size(); i++) {
            Tab tab = tabs.get(i);
            assertEquals(isIncognito, tab.isIncognito());
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> filter.mergeTabsToGroup(tab.getId(), rootTab.getId()));
        }
    }

    /**
     * Create a {@link OverviewModeBehaviorWatcher} to inspect overview show.
     */
    static OverviewModeBehaviorWatcher createOverviewShowWatcher(ChromeTabbedActivity cta) {
        return new OverviewModeBehaviorWatcher(cta.getLayoutManager(), true, false);
    }

    /**
     * Create a {@link OverviewModeBehaviorWatcher} to inspect overview hide.
     */
    static OverviewModeBehaviorWatcher createOverviewHideWatcher(ChromeTabbedActivity cta) {
        return new OverviewModeBehaviorWatcher(cta.getLayoutManager(), false, true);
    }

    /**
     * Rotate device to the target orientation. Do nothing if the screen is already in that
     * orientation.
     * @param cta             The current running activity.
     * @param orientation     The target orientation we want the screen to rotate to.
     */
    static void rotateDeviceToOrientation(ChromeTabbedActivity cta, int orientation) {
        if (cta.getResources().getConfiguration().orientation == orientation) return;
        assertTrue(orientation == Configuration.ORIENTATION_LANDSCAPE
                || orientation == Configuration.ORIENTATION_PORTRAIT);
        cta.setRequestedOrientation(orientation == Configuration.ORIENTATION_LANDSCAPE
                        ? ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
                        : ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        CriteriaHelper.pollUiThread(
                () -> orientation == cta.getResources().getConfiguration().orientation);
    }

    /**
     * @return whether current running API is lower or equal to KitKat.
     */
    static boolean isKitKatAndBelow() {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP;
    }

    /**
     * @return whether animators are enabled on device by checking whether the animation duration
     * scale is set to 0.0.
     */
    public static boolean areAnimatorsEnabled() {
        // We default to assuming that animations are enabled in case ANIMATOR_DURATION_SCALE is not
        // defined.
        final float defaultScale = 1f;
        float durationScale =
                Settings.Global.getFloat(ContextUtils.getApplicationContext().getContentResolver(),
                        Settings.Global.ANIMATOR_DURATION_SCALE, defaultScale);
        return !(durationScale == 0.0);
    }

    /**
     * Make Chrome have {@code numTabs} of regular Tabs and {@code numIncognitoTabs} of incognito
     * tabs with {@code url} loaded.
     * @param rule The {@link ChromeTabbedActivityTestRule}.
     * @param numTabs The number of regular tabs.
     * @param numIncognitoTabs The number of incognito tabs.
     * @param url The URL to load.
     */
    public static void prepareTabsWithThumbnail(ChromeTabbedActivityTestRule rule, int numTabs,
            int numIncognitoTabs, @Nullable String url) {
        assertTrue(numTabs >= 1);
        assertTrue(numIncognitoTabs >= 0);

        assertEquals(1, rule.getActivity().getTabModelSelector().getModel(false).getCount());
        assertEquals(0, rule.getActivity().getTabModelSelector().getModel(true).getCount());

        if (url != null) rule.loadUrl(url);
        if (numTabs > 1) {
            // When Chrome started, there is already one Tab created by default.
            createTabsWithThumbnail(rule, numTabs - 1, url, false);
        }
        if (numIncognitoTabs > 0) createTabsWithThumbnail(rule, numIncognitoTabs, url, true);

        assertEquals(numTabs, rule.getActivity().getTabModelSelector().getModel(false).getCount());
        assertEquals(numIncognitoTabs,
                rule.getActivity().getTabModelSelector().getModel(true).getCount());
        if (url != null) {
            verifyAllTabsHaveUrl(rule.getActivity().getTabModelSelector().getModel(false), url);
            verifyAllTabsHaveUrl(rule.getActivity().getTabModelSelector().getModel(true), url);
        }
    }

    private static void verifyAllTabsHaveUrl(TabModel tabModel, String url) {
        for (int i = 0; i < tabModel.getCount(); i++) {
            assertEquals(url, tabModel.getTabAt(i).getUrl());
        }
    }

    /**
     * Create {@code numTabs} of {@link Tab}s with {@code url} loaded to Chrome.
     * Note that if the test doesn't care about thumbnail, use {@link TabUiTestHelper#createTabs}
     * instead since it's faster.
     *
     * @param rule The {@link ChromeTabbedActivityTestRule}.
     * @param numTabs The number of tabs to create.
     * @param url The URL to load. Skip loading when null, but the thumbnail for the NTP might not
     *            be saved.
     * @param isIncognito Whether the tab is incognito tab.
     */
    private static void createTabsWithThumbnail(ChromeTabbedActivityTestRule rule, int numTabs,
            @Nullable String url, boolean isIncognito) {
        assertTrue(numTabs >= 1);

        int previousTabCount =
                rule.getActivity().getTabModelSelector().getModel(isIncognito).getCount();

        for (int i = 0; i < numTabs; i++) {
            TabModel previousTabModel = rule.getActivity().getTabModelSelector().getCurrentModel();
            int previousTabIndex = previousTabModel.index();
            Tab previousTab = previousTabModel.getTabAt(previousTabIndex);

            ChromeTabUtils.newTabFromMenu(InstrumentationRegistry.getInstrumentation(),
                    rule.getActivity(), isIncognito, url == null);

            if (url != null) rule.loadUrl(url);

            TabModel currentTabModel = rule.getActivity().getTabModelSelector().getCurrentModel();
            int currentTabIndex = currentTabModel.index();

            boolean fixPendingReadbacks =
                    rule.getActivity().getTabContentManager().getPendingReadbacksForTesting() != 0;

            // When there are pending readbacks due to detached Tabs, try to fix it by switching
            // back to that tab.
            if (fixPendingReadbacks && previousTabIndex != TabModel.INVALID_TAB_INDEX) {
                // clang-format off
                TestThreadUtils.runOnUiThreadBlocking(() ->
                        previousTabModel.setIndex(previousTabIndex, TabSelectionType.FROM_USER)
                );
                // clang-format on
            }

            checkThumbnailsExist(previousTab);

            if (fixPendingReadbacks) {
                // clang-format off
                TestThreadUtils.runOnUiThreadBlocking(() -> currentTabModel.setIndex(
                        currentTabIndex, TabSelectionType.FROM_USER)
                );
                // clang-format on
            }
        }

        ChromeTabUtils.waitForTabPageLoaded(
                rule.getActivity().getActivityTab(), null, null, WAIT_TIMEOUT_SECONDS * 10);

        assertEquals(numTabs + previousTabCount,
                rule.getActivity().getTabModelSelector().getModel(isIncognito).getCount());

        CriteriaHelper.pollUiThread(Criteria.equals(0,
                () -> rule.getActivity().getTabContentManager().getPendingReadbacksForTesting()));
    }

    public static void verifyAllTabsHaveThumbnail(TabModel tabModel) {
        for (int i = 0; i < tabModel.getCount(); i++) {
            checkThumbnailsExist(tabModel.getTabAt(i));
        }
    }

    public static void checkThumbnailsExist(Tab tab) {
        File etc1File = TabContentManager.getTabThumbnailFileEtc1(tab);
        CriteriaHelper.pollInstrumentationThread(etc1File::exists,
                "The thumbnail " + etc1File.getName() + " is not found",
                DEFAULT_MAX_TIME_TO_POLL * 10, DEFAULT_POLLING_INTERVAL);

        File jpegFile = TabContentManager.getTabThumbnailFileJpeg(tab);
        CriteriaHelper.pollInstrumentationThread(jpegFile::exists,
                "The thumbnail " + jpegFile.getName() + " is not found",
                DEFAULT_MAX_TIME_TO_POLL * 10, DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Implementation of {@link ViewAssertion} to verify the {@link RecyclerView} has correct number
     * of children, and children are showing correctly.
     */
    public static class CardCountAssertion implements ViewAssertion {
        private int mExpectedCount;

        public static CardCountAssertion havingTabCount(int tabCount) {
            return new CardCountAssertion(tabCount);
        }

        public CardCountAssertion(int expectedCount) {
            mExpectedCount = expectedCount;
        }

        @Override
        public void check(View view, NoMatchingViewException noMatchException) {
            if (noMatchException != null) throw noMatchException;

            RecyclerView recyclerView = ((RecyclerView) view);
            RecyclerView.Adapter adapter = recyclerView.getAdapter();
            CriteriaHelper.pollUiThread(
                    Criteria.equals(mExpectedCount, () -> adapter.getItemCount()));

            for (int i = 0; i < mExpectedCount; i++) {
                RecyclerView.ViewHolder viewHolder =
                        recyclerView.findViewHolderForAdapterPosition(i);
                if (viewHolder == null) return;
                // This is to check if dialog hiding animation plays properly.
                assertTrue(1f == viewHolder.itemView.getAlpha());
                assertEquals(View.VISIBLE, viewHolder.itemView.getVisibility());
            }
        }
    }
}
