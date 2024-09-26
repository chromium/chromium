// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.CallbackHelper.WAIT_TIMEOUT_SECONDS;
import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL;
import static org.chromium.base.test.util.CriteriaHelper.DEFAULT_POLLING_INTERVAL;
import static org.chromium.components.browser_ui.widget.RecyclerViewTestUtils.waitForStableRecyclerView;

import android.app.Activity;
import android.content.Context;
import android.graphics.drawable.BitmapDrawable;
import android.provider.Settings;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.core.view.ViewCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.NoMatchingRootException;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.ViewAssertion;
import androidx.test.espresso.action.GeneralLocation;
import androidx.test.espresso.action.GeneralSwipeAction;
import androidx.test.espresso.action.Press;
import androidx.test.espresso.action.Swipe;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matcher;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.HubLayout;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tab_ui.TabUiThemeUtils;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ContentUrlConstants;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/** Utilities helper class for tab grid/group tests. */
public class TabUiTestHelper {
    /**
     * Create {@code tabsCount} tabs for {@code cta} in certain tab model based on {@code
     * isIncognito}.
     * @param cta            A current running activity to create tabs.
     * @param isIncognito    Indicator for whether to create tabs in normal model or incognito
     *         model.
     * @param tabsCount      Number of tabs to be created.
     */
    public static void createTabs(ChromeTabbedActivity cta, boolean isIncognito, int tabsCount) {
        for (int i = 0; i < (isIncognito ? tabsCount : tabsCount - 1); i++) {
            ChromeTabUtils.newTabFromMenu(
                    InstrumentationRegistry.getInstrumentation(), cta, isIncognito, true);
        }
    }

    /**
     * Open additional tabs for the provided activity. The added tabs will be opened to
     * "about:blank" and will not wait for the page to finish loading.
     *
     * @param cta The activity to add the tabs to.
     * @param incognito Whether the tabs should be incognito.
     * @param count The number of tabs to create.
     */
    public static void addBlankTabs(ChromeTabbedActivity cta, boolean incognito, int count) {
        for (int i = 0; i < count; i++) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        cta.getTabCreator(incognito)
                                .createNewTab(
                                        new LoadUrlParams(
                                                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL),
                                        TabLaunchType.FROM_CHROME_UI,
                                        null);
                    });
        }
    }

    /**
     * Enter tab switcher from a tab page.
     *
     * @param cta The current running activity.
     */
    public static void enterTabSwitcher(ChromeTabbedActivity cta) {
        assertFalse(cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER));
        // TODO(crbug.com/40155797): Replace this with clicking tab switcher button via espresso.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    cta.findViewById(R.id.tab_switcher_button).performClick();
                });
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.TAB_SWITCHER);
    }

    /**
     * Leave tab switcher by tapping "back".
     * @param cta  The current running activity.
     */
    public static void leaveTabSwitcher(ChromeTabbedActivity cta) {
        LayoutManagerChrome layoutManager = cta.getLayoutManager();
        LayoutTestUtils.waitForLayout(layoutManager, LayoutType.TAB_SWITCHER);
        // Back press may resolve differently during the show/hide animations. Don't call this until
        // we are certain the layout is visible.
        CallbackHelper finishedHidingCallbackHelper = new CallbackHelper();
        LayoutStateObserver observer =
                new LayoutStateObserver() {
                    @Override
                    public void onFinishedHiding(int layoutType) {
                        if (layoutType != LayoutType.TAB_SWITCHER) return;

                        finishedHidingCallbackHelper.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    layoutManager.addObserver(observer);
                    cta.onBackPressed();
                });
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
        try {
            finishedHidingCallbackHelper.waitForOnly();
        } catch (TimeoutException e) {
            throw new AssertionError("LayoutType.TAB_SWITCHER never finished hiding.", e);
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    layoutManager.removeObserver(observer);
                });
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
        clickTabSwitcherCardWithParent(cta, index, getTabSwitcherAncestorId(cta));
    }

    private static void clickTabSwitcherCardWithParent(
            ChromeTabbedActivity cta, int index, int parentId) {
        assertTrue(cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER));
        onView(allOf(isDescendantOfA(withId(parentId)), withId(R.id.tab_list_recycler_view)))
                .perform(RecyclerViewActions.actionOnItemAtPosition(index, click()));
    }

    /**
     * Click the first tab in tab grid dialog to open a tab page.
     * @param cta  The current running activity.
     */
    static void clickFirstTabInDialog(ChromeTabbedActivity cta) {
        clickNthTabInDialog(cta, 0);
    }

    /**
     * Click the Nth tab in tab grid dialog to open a tab page.
     * @param cta  The current running activity.
     * @param index The index of the target tab.
     */
    static void clickNthTabInDialog(ChromeTabbedActivity cta, int index) {
        onView(
                        allOf(
                                withId(R.id.tab_list_recycler_view),
                                withParent(withId(R.id.tab_grid_dialog_recycler_view_container))))
                .perform(RecyclerViewActions.actionOnItemAtPosition(index, click()));
        LayoutTestUtils.waitForLayout(cta.getLayoutManager(), LayoutType.BROWSING);
    }

    /**
     * Close the first tab in tab gri dialog.
     * @param cta  The current running activity.
     */
    static void closeFirstTabInDialog() {
        closeNthTabInDialog(0);
    }

    /**
     * Close the Nth tab in tab gri dialog.
     * @param index The index of the target tab to close.
     */
    static void closeNthTabInDialog(int index) {
        onView(
                        allOf(
                                withId(R.id.tab_list_recycler_view),
                                withParent(withId(R.id.tab_grid_dialog_recycler_view_container))))
                .perform(
                        new ViewAction() {
                            @Override
                            public Matcher<View> getConstraints() {
                                return isDisplayed();
                            }

                            @Override
                            public String getDescription() {
                                return "close tab with index " + index;
                            }

                            @Override
                            public void perform(UiController uiController, View view) {
                                RecyclerView recyclerView = (RecyclerView) view;
                                RecyclerView.ViewHolder viewHolder =
                                        recyclerView.findViewHolderForAdapterPosition(index);
                                assert viewHolder != null;
                                viewHolder.itemView.findViewById(R.id.action_button).performClick();
                            }
                        });
    }

    /** Close the first tab in grid tab switcher. */
    public static void closeFirstTabInTabSwitcher(Context context) {
        closeNthTabInTabSwitcher(context, 0);
    }

    /**
     * Close a tab group. Depending on feature setting, the action button might be a simple x or a
     * three dot into a menu.
     */
    public static void closeFirstTabGroupInTabSwitcher(Context context) {
        closeFirstTabInTabSwitcher(context);
        if (ChromeFeatureList.sTabGroupPaneAndroid.isEnabled()) {
            onView(withText("Close")).perform(click());
        }
    }

    /**
     * Close the Nth tab in grid tab switcher.
     *
     * @param context The activity context.
     * @param index The index of the target tab to close.
     */
    static void closeNthTabInTabSwitcher(Context context, int index) {
        onView(
                        allOf(
                                isDescendantOfA(withId(getTabSwitcherAncestorId(context))),
                                withId(R.id.tab_list_recycler_view)))
                .perform(
                        new ViewAction() {
                            @Override
                            public Matcher<View> getConstraints() {
                                return isDisplayed();
                            }

                            @Override
                            public String getDescription() {
                                return "close tab with index " + index;
                            }

                            @Override
                            public void perform(UiController uiController, View view) {
                                RecyclerView recyclerView = (RecyclerView) view;
                                RecyclerView.ViewHolder viewHolder =
                                        recyclerView.findViewHolderForAdapterPosition(index);
                                assert viewHolder != null;
                                viewHolder.itemView.findViewById(R.id.action_button).performClick();
                            }
                        });
    }

    /**
     * Check whether the tab list in {@link android.widget.PopupWindow} is completely showing. This
     * can be used for tab grid dialog and tab group popup UI.
     *
     * @param cta The current running activity.
     * @return Whether the tab list in a popup component is completely showing.
     */
    static boolean isPopupTabListCompletelyShowing(ChromeTabbedActivity cta) {
        boolean isShowing = true;
        try {
            onView(withId(R.id.tab_list_recycler_view))
                    .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                    .check(matches(isCompletelyDisplayed()))
                    .check((v, e) -> assertEquals(1f, v.getAlpha(), 0.0));
        } catch (NoMatchingRootException | AssertionError e) {
            isShowing = false;
        } catch (Exception e) {
            assert false : "error when inspecting pop up tab list.";
        }
        return isShowing;
    }

    /**
     * Check whether the tab list in {@link android.widget.PopupWindow} is completely hidden. This
     * can be used for tab grid dialog and tab group popup UI.
     * @param cta  The current running activity.
     * @return Whether the tab list in a popup component is completely hidden.
     */
    static boolean isPopupTabListCompletelyHidden(ChromeTabbedActivity cta) {
        boolean isHidden = false;
        try {
            onView(withId(R.id.tab_list_recycler_view))
                    .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                    .check(matches(isDisplayed()));
        } catch (NoMatchingRootException e) {
            isHidden = true;
        } catch (Exception e) {
            assert false : "error when inspecting pop up tab list.";
        }
        return isHidden;
    }

    /**
     * Verify the number of tabs in the tab list showing in a popup component.
     * @param cta   The current running activity.
     * @param count The count of the tabs in the tab list.
     */
    static void verifyShowingPopupTabList(ChromeTabbedActivity cta, int count) {
        onView(withId(R.id.tab_list_recycler_view))
                .inRoot(withDecorView(not(cta.getWindow().getDecorView())))
                .check(ChildrenCountAssertion.havingTabCount(count));
    }

    /**
     * Merge all normal tabs into a single tab group.
     * @param cta   The current running activity.
     */
    public static void mergeAllNormalTabsToAGroup(ChromeTabbedActivity cta) {
        mergeAllTabsToAGroup(cta, false);
    }

    /**
     * Merge all incognito tabs into a single tab group.
     * @param cta   The current running activity.
     */
    static void mergeAllIncognitoTabsToAGroup(ChromeTabbedActivity cta) {
        mergeAllTabsToAGroup(cta, true);
    }

    /**
     * Merge all tabs in one tab model into a single tab group.
     * @param cta           The current running activity.
     * @param isIncognito   indicates the tab model that we are creating tab group in.
     */
    static void mergeAllTabsToAGroup(ChromeTabbedActivity cta, boolean isIncognito) {
        List<Tab> tabGroup = new ArrayList<>();
        TabModel tabModel = cta.getTabModelSelector().getModel(isIncognito);
        for (int i = 0; i < tabModel.getCount(); i++) {
            tabGroup.add(tabModel.getTabAt(i));
        }
        createTabGroup(cta, isIncognito, tabGroup);
        assertTrue(
                cta.getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                        instanceof TabGroupModelFilter);
        TabGroupModelFilter filter =
                (TabGroupModelFilter)
                        cta.getTabModelSelector()
                                .getTabModelFilterProvider()
                                .getTabModelFilter(isIncognito);
        assertEquals(1, filter.getCount());
    }

    /**
     * Verify that current tab models hold correct number of tabs.
     * @param cta            The current running activity.
     * @param normalTabs     The correct number of normal tabs.
     * @param incognitoTabs  The correct number of incognito tabs.
     */
    public static void verifyTabModelTabCount(
            ChromeTabbedActivity cta, int normalTabs, int incognitoTabs) {
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            cta.getTabModelSelector().getModel(false).getCount(), is(normalTabs));
                });
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            cta.getTabModelSelector().getModel(true).getCount(), is(incognitoTabs));
                });
    }

    /**
     * Verify there are correct number of cards in tab switcher.
     * @param cta       The current running activity.
     * @param count     The correct number of cards in tab switcher.
     */
    public static void verifyTabSwitcherCardCount(ChromeTabbedActivity cta, int count) {
        assertTrue(cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER));
        int viewHolder = getTabSwitcherAncestorId(cta);
        onView(allOf(isDescendantOfA(withId(viewHolder)), withId(R.id.tab_list_recycler_view)))
                .check(ChildrenCountAssertion.havingTabCount(count));
    }

    /**
     * Returns an ancestor View ID for the tab switcher based on form factor and feature enabled.
     * Should be used with the {@link isDescendantOfA} view matcher.
     *
     * @param context The activity context.
     * @return View ID of the a nearby ancestor view.
     */
    public static int getTabSwitcherAncestorId(Context context) {
        return org.chromium.chrome.browser.hub.R.id.hub_pane_host;
    }

    /**
     * Verify there are correct number of favicons in tab strip.
     *
     * @param cta The current running activity.
     * @param count The correct number of favicons in tab strip.
     */
    static void verifyTabStripFaviconCount(ChromeTabbedActivity cta, int count) {
        assertFalse(cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER));
        onView(
                        allOf(
                                withParent(withId(R.id.toolbar_container_view)),
                                withId(R.id.tab_list_recycler_view)))
                .check(ChildrenCountAssertion.havingTabCount(count));
    }

    /**
     * Create a tab group using {@code tabs}.
     * @param cta             The current running activity.
     * @param isIncognito     Whether the group is in normal model or incognito model.
     * @param tabs            A list of {@link Tab} to create group.
     */
    public static void createTabGroup(
            ChromeTabbedActivity cta, boolean isIncognito, List<Tab> tabs) {
        if (tabs.size() == 0) return;
        assert cta.getTabModelSelector().getTabModelFilterProvider().getCurrentTabModelFilter()
                instanceof TabGroupModelFilter;
        TabGroupModelFilter filter =
                (TabGroupModelFilter)
                        cta.getTabModelSelector()
                                .getTabModelFilterProvider()
                                .getTabModelFilter(isIncognito);
        Tab rootTab = tabs.get(0);
        for (int i = 1; i < tabs.size(); i++) {
            Tab tab = tabs.get(i);
            assertEquals(isIncognito, tab.isIncognito());
            ThreadUtils.runOnUiThreadBlocking(
                    () -> filter.mergeTabsToGroup(tab.getId(), rootTab.getId()));
        }
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
                Settings.Global.getFloat(
                        ContextUtils.getApplicationContext().getContentResolver(),
                        Settings.Global.ANIMATOR_DURATION_SCALE,
                        defaultScale);
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
    public static void prepareTabsWithThumbnail(
            ChromeTabbedActivityTestRule rule,
            int numTabs,
            int numIncognitoTabs,
            @Nullable String url) {
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
        assertEquals(
                numIncognitoTabs,
                rule.getActivity().getTabModelSelector().getModel(true).getCount());
        if (url != null) {
            verifyAllTabsHaveUrl(rule.getActivity().getTabModelSelector().getModel(false), url);
            verifyAllTabsHaveUrl(rule.getActivity().getTabModelSelector().getModel(true), url);
        }
    }

    private static void verifyAllTabsHaveUrl(TabModel tabModel, String url) {
        for (int i = 0; i < tabModel.getCount(); i++) {
            assertEquals(url, ChromeTabUtils.getUrlStringOnUiThread(tabModel.getTabAt(i)));
        }
    }

    /**
     * Create {@code numTabs} of {@link Tab}s with {@code url} loaded to Chrome. Note that if the
     * test doesn't care about thumbnail, use {@link TabUiTestHelper#createTabs} instead since it's
     * faster.
     *
     * @param rule The {@link ChromeTabbedActivityTestRule}.
     * @param numTabs The number of tabs to create.
     * @param url The URL to load. Skip loading when null, but the thumbnail for the NTP might not
     *     be saved.
     * @param isIncognito Whether the tab is incognito tab.
     */
    public static void createTabsWithThumbnail(
            ChromeTabbedActivityTestRule rule,
            int numTabs,
            @Nullable String url,
            boolean isIncognito) {
        ChromeTabbedActivity cta = rule.getActivity();
        assertTrue(numTabs >= 1);
        int previousTabCount =
                rule.getActivity().getTabModelSelector().getModel(isIncognito).getCount();

        for (int i = 0; i < numTabs; i++) {
            TabModel previousTabModel = cta.getTabModelSelector().getCurrentModel();
            int previousTabIndex = previousTabModel.index();
            Tab previousTab = previousTabModel.getTabAt(previousTabIndex);

            boolean urlIsNull = url == null;
            if (urlIsNull) {
                ChromeTabUtils.newTabFromMenu(
                        InstrumentationRegistry.getInstrumentation(), cta, isIncognito, urlIsNull);
            } else {
                ChromeTabUtils.fullyLoadUrlInNewTab(
                        InstrumentationRegistry.getInstrumentation(), cta, url, isIncognito);
            }

            TabModel currentTabModel = cta.getTabModelSelector().getCurrentModel();
            int currentTabIndex = currentTabModel.index();
            Tab currentTab = currentTabModel.getTabAt(currentTabIndex);

            waitForThumbnailsToCapture(cta, previousTab, currentTab);
        }

        ChromeTabUtils.waitForTabPageLoaded(
                rule.getActivity().getActivityTab(), null, null, WAIT_TIMEOUT_SECONDS * 3);

        assertEquals(
                numTabs + previousTabCount,
                rule.getActivity().getTabModelSelector().getModel(isIncognito).getCount());

        // Don't wait on the current tab to fetch. It should be fetched either when entering the
        // tab switcher or as a side-effect of unsticking the last tab.
    }

    private static void waitForThumbnailsToCapture(
            ChromeTabbedActivity cta, Tab previousTab, Tab currentTab) {
        if (previousTab == null) return;

        boolean fixPendingReadbacks = false;
        try {
            // Wait for the capture.
            CriteriaHelper.pollUiThread(
                    () -> {
                        return !cta.getTabContentManager()
                                .isTabCaptureInFlightForTesting(previousTab.getId());
                    });
        } catch (CriteriaHelper.TimeoutException e) {
            // Capture is likely stuck.
            fixPendingReadbacks = true;
        }
        // Return to the tab to try to get a surface back for the tab so the capture can finish.
        if (fixPendingReadbacks) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        TabModelUtils.selectTabById(
                                cta.getTabModelSelector(),
                                previousTab.getId(),
                                TabSelectionType.FROM_USER);
                    });
        }
        // Poll for the file to exist on disk.
        checkThumbnailsExist(previousTab);
        // Return to the previous tab.
        if (fixPendingReadbacks && currentTab != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        TabModelUtils.selectTabById(
                                cta.getTabModelSelector(),
                                currentTab.getId(),
                                TabSelectionType.FROM_USER);
                    });
        }
    }

    public static void verifyAllTabsHaveThumbnail(TabModel tabModel) {
        for (int i = 0; i < tabModel.getCount(); i++) {
            checkThumbnailsExist(tabModel.getTabAt(i));
        }
    }

    public static void waitForThumbnailsToFetch(RecyclerView recyclerView) {
        assertTrue(recyclerView instanceof TabListRecyclerView);
        CriteriaHelper.pollUiThread(
                () -> {
                    boolean allFetched = true;
                    int i = 0;
                    LinearLayoutManager layoutManager =
                            (LinearLayoutManager) recyclerView.getLayoutManager();
                    for (i = layoutManager.findFirstVisibleItemPosition();
                            i <= layoutManager.findLastVisibleItemPosition();
                            i++) {
                        View v = layoutManager.findViewByPosition(i);
                        TabThumbnailView thumbnail = v.findViewById(R.id.tab_thumbnail);

                        // Some items may not be cards or may not have thumbnails.
                        if (thumbnail == null) continue;

                        if (thumbnail.isPlaceholder()
                                || !(thumbnail.getDrawable() instanceof BitmapDrawable)
                                || ((BitmapDrawable) thumbnail.getDrawable()).getBitmap() == null) {
                            allFetched = false;
                            break;
                        }
                    }
                    Criteria.checkThat(
                            "The thumbnail for card at position " + i + " is missing.",
                            allFetched,
                            is(true));
                });
    }

    public static void checkThumbnailsExist(Tab tab) {
        File etc1File = TabContentManager.getTabThumbnailFileEtc1(tab);
        CriteriaHelper.pollInstrumentationThread(
                etc1File::exists,
                "The thumbnail " + etc1File.getName() + " is not found",
                DEFAULT_MAX_TIME_TO_POLL * 10,
                DEFAULT_POLLING_INTERVAL);

        File jpegFile = TabContentManager.getTabThumbnailFileJpeg(tab.getId());
        CriteriaHelper.pollInstrumentationThread(
                jpegFile::exists,
                "The thumbnail " + jpegFile.getName() + " is not found",
                DEFAULT_MAX_TIME_TO_POLL * 10,
                DEFAULT_POLLING_INTERVAL);
    }

    /**
     * Verify that the snack bar is showing and click on the snack bar button. Right now it is only
     * used for undoing a tab closure or undoing a tab group merge. This should be used with
     * CriteriaHelper.pollInstrumentationThread().
     * @return whether the visibility checking and the clicking have finished or not.
     */
    public static boolean verifyUndoBarShowingAndClickUndo() {
        boolean hasClicked = true;
        try {
            onView(withId(R.id.snackbar_button)).check(matches(isCompletelyDisplayed()));
            onView(withId(R.id.snackbar_button)).perform(click());
        } catch (NoMatchingRootException | AssertionError e) {
            hasClicked = false;
        } catch (Exception e) {
            assert false : "error when verifying undo snack bar.";
        }
        return hasClicked;
    }

    /**
     * Get the {@link GeneralSwipeAction} used to perform a swipe-to-dismiss action in tab grid
     * layout.
     * @param isLeftToRight  decides whether the swipe is from left to right or from right to left.
     * @return {@link GeneralSwipeAction} to perform swipe-to-dismiss.
     */
    public static GeneralSwipeAction getSwipeToDismissAction(boolean isLeftToRight) {
        if (isLeftToRight) {
            return new GeneralSwipeAction(
                    Swipe.FAST,
                    GeneralLocation.CENTER_LEFT,
                    GeneralLocation.CENTER_RIGHT,
                    Press.FINGER);
        } else {
            return new GeneralSwipeAction(
                    Swipe.FAST,
                    GeneralLocation.CENTER_RIGHT,
                    GeneralLocation.CENTER_LEFT,
                    Press.FINGER);
        }
    }

    /** Finishes the given activity and do tab_ui-specific cleanup. */
    public static void finishActivity(final Activity activity) throws Exception {
        ApplicationTestUtils.finishActivity(activity);
    }

    /**
     * Click on the incognito toggle within grid tab switcher top toolbar to switch between normal
     * and incognito tab model.
     * @param cta          The current running activity.
     * @param isIncognito  indicates whether the incognito or normal tab model is selected after
     *         switch.
     */
    public static void switchTabModel(ChromeTabbedActivity cta, boolean isIncognito) {
        assertTrue(isIncognito != cta.getTabModelSelector().isIncognitoSelected());
        assertTrue(cta.getLayoutManager().isLayoutVisible(LayoutType.TAB_SWITCHER));

        // The non-incognito contentDescription is a substring found in the following string:
        // R.string.accessibility_tab_switcher_standard_stack.
        String contentDescription =
                isIncognito
                        ? cta.getString(R.string.accessibility_tab_switcher_incognito_stack)
                        : "standard tab";
        onView(
                        allOf(
                                isDescendantOfA(
                                        withId(org.chromium.chrome.browser.hub.R.id.hub_toolbar)),
                                withContentDescription(containsString(contentDescription))))
                .perform(click());

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            cta.getTabModelSelector().isIncognitoSelected(), is(isIncognito));
                });
        // Wait for tab list recyclerView to finish animation after tab model switch.
        ViewGroup viewGroup = cta.findViewById(getTabSwitcherAncestorId(cta));
        RecyclerView recyclerView = viewGroup.findViewById(R.id.tab_list_recycler_view);
        waitForStableRecyclerView(recyclerView);
    }

    /**
     * Infers whether the tab is currently selected, by making assumptions about what the view state
     * should look like. This method is fairly fragile to changes in implementation. Note that this
     * does not handle tab list views correctly.
     * @param holder The root of the tab {@link View} objects.
     * @return Whether the tab is currently selected.
     */
    public static boolean isTabViewSelected(ViewGroup holder) {
        View cardView = holder.findViewById(R.id.card_view);
        final @ColorInt int actualColor =
                ViewCompat.getBackgroundTintList(cardView).getDefaultColor();
        final @ColorInt int selectedColor =
                TabUiThemeUtils.getCardViewBackgroundColor(
                        holder.getContext(), /* isIncognito= */ false, /* isSelected= */ true);
        return actualColor == selectedColor;
    }

    /**
     * Implementation of {@link ViewAssertion} to verify the {@link RecyclerView} has correct number
     * of children.
     */
    public static class ChildrenCountAssertion implements ViewAssertion {
        @IntDef({ChildrenType.TAB, ChildrenType.TAB_SUGGESTION_MESSAGE})
        @Retention(RetentionPolicy.SOURCE)
        public @interface ChildrenType {
            int TAB = 0;
            int TAB_SUGGESTION_MESSAGE = 1;
        }

        private int mExpectedCount;
        @ChildrenType private int mExpectedChildrenType;

        public static ChildrenCountAssertion havingTabCount(int tabCount) {
            return new ChildrenCountAssertion(ChildrenType.TAB, tabCount);
        }

        public static ChildrenCountAssertion havingTabSuggestionMessageCardCount(int count) {
            return new ChildrenCountAssertion(ChildrenType.TAB_SUGGESTION_MESSAGE, count);
        }

        public ChildrenCountAssertion(@ChildrenType int expectedChildrenType, int expectedCount) {
            mExpectedChildrenType = expectedChildrenType;
            mExpectedCount = expectedCount;
        }

        @Override
        public void check(View view, NoMatchingViewException noMatchException) {
            if (noMatchException != null) throw noMatchException;

            switch (mExpectedChildrenType) {
                case ChildrenType.TAB:
                    checkTabCount(view);
                    break;
                case ChildrenType.TAB_SUGGESTION_MESSAGE:
                    checkTabSuggestionMessageCard(view);
                    break;
            }
        }

        private void checkTabCount(View view) {
            RecyclerView recyclerView = ((RecyclerView) view);
            recyclerView.setItemAnimator(null); // Disable animation to reduce flakiness.
            RecyclerView.Adapter adapter = recyclerView.getAdapter();

            int itemCount = adapter.getItemCount();
            int nonTabCardCount = 0;

            for (int i = 0; i < itemCount; i++) {
                RecyclerView.ViewHolder viewHolder =
                        recyclerView.findViewHolderForAdapterPosition(i);
                if (viewHolder == null) return;
                if (viewHolder.getItemViewType() != TabProperties.UiType.TAB
                        && viewHolder.getItemViewType() != TabProperties.UiType.STRIP) {
                    nonTabCardCount += 1;
                }
            }
            assertEquals(mExpectedCount + nonTabCardCount, itemCount);
        }

        private void checkTabSuggestionMessageCard(View view) {
            RecyclerView recyclerView = ((RecyclerView) view);
            recyclerView.setItemAnimator(null); // Disable animation to reduce flakiness.
            RecyclerView.Adapter adapter = recyclerView.getAdapter();

            int itemCount = adapter.getItemCount();
            int tabSuggestionMessageCount = 0;

            for (int i = 0; i < itemCount; i++) {
                RecyclerView.ViewHolder viewHolder =
                        recyclerView.findViewHolderForAdapterPosition(i);
                if (viewHolder == null) return;
                if (viewHolder.getItemViewType() == TabProperties.UiType.MESSAGE) {
                    tabSuggestionMessageCount += 1;
                }
            }
            assertEquals(mExpectedCount, tabSuggestionMessageCount);
        }
    }

    /**
     * Verifies whether the correct layout is created for the tab switcher in LayoutManagerChrome.
     */
    public static void verifyTabSwitcherLayoutType(ChromeTabbedActivity cta) {
        getTabSwitcherLayoutAndVerify(cta);
    }

    /**
     * Gets the tab switcher layout depends on whether the refactoring is enabled and verifies its
     * type. If refactoring is enabled this will trigger lazy init of the tab switcher layout.
     */
    public static Layout getTabSwitcherLayoutAndVerify(ChromeTabbedActivity cta) {
        final LayoutManagerChrome layoutManager = cta.getLayoutManager();
        Layout layout = layoutManager.getHubLayoutForTesting();
        if (layout == null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        layoutManager.initHubLayoutForTesting();
                    });
        }
        layout = layoutManager.getHubLayoutForTesting();
        assertTrue(layout instanceof HubLayout);
        return layout;
    }
}
