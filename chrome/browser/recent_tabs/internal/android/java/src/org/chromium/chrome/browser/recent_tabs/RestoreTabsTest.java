// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.pressBack;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.mockito.ArgumentMatchers.anyList;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.os.Build;

import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/** Integration tests for the RestoreTabs feature. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceFormFactor.PHONE)
@DisableFeatures(ChromeFeatureList.ANDROID_HUB_SEARCH)
@DoNotBatch(reason = "Tests startup behaviors that trigger per-session")
public class RestoreTabsTest {
    private static final String RESTORE_TABS_FEATURE = FeatureConstants.RESTORE_TABS_ON_FRE_FEATURE;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public JniMocker jniMocker = new JniMocker();

    @Spy ForeignSessionHelper.Natives mForeignSessionHelperJniSpy;
    // Tell R8 not to break the ability to mock the class.
    @Spy ForeignSessionHelperJni mUnused;

    @Mock private Tracker mMockTracker;

    private BottomSheetController mBottomSheetController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        TrackerFactory.setTrackerForTests(mMockTracker);

        mForeignSessionHelperJniSpy = Mockito.spy(ForeignSessionHelperJni.get());
        jniMocker.mock(ForeignSessionHelperJni.TEST_HOOKS, mForeignSessionHelperJniSpy);
        doReturn(true).when(mForeignSessionHelperJniSpy).isTabSyncEnabled(anyLong());

        mBottomSheetController =
                mActivityTestRule
                        .getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getBottomSheetController();
    }

    @After
    public void tearDown() {
        TrackerFactory.setTrackerForTests(null);
    }

    @Test
    @MediumTest
    @DisableIf.Build(
            supported_abis_includes = "armeabi-v7a",
            sdk_is_less_than = Build.VERSION_CODES.O,
            message = "Flaky only on test-n-phone, crbug.com/1469008")
    public void testRestoreTabsPromo_triggerBottomSheetView() {
        // Setup mock data
        ForeignSessionTab tab = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        List<ForeignSessionTab> tabs = new ArrayList<>();
        tabs.add(tab);
        ForeignSessionWindow window = new ForeignSessionWindow(31L, 1, tabs);
        List<ForeignSessionWindow> windows = new ArrayList<>();
        windows.add(window);
        ForeignSession session =
                new ForeignSession("tag", "John's iPhone 6", 32L, windows, FormFactor.PHONE);
        List<ForeignSession> sessions = new ArrayList<>();
        sessions.add(session);

        doReturn(true).when(mMockTracker).wouldTriggerHelpUI(eq(RESTORE_TABS_FEATURE));
        doReturn(true).when(mMockTracker).shouldTriggerHelpUI(eq(RESTORE_TABS_FEATURE));
        doAnswer(
                        invocation -> {
                            List<ForeignSession> invoked_sessions = invocation.getArgument(1);
                            invoked_sessions.addAll(sessions);
                            return true;
                        })
                .when(mForeignSessionHelperJniSpy)
                .getMobileAndTabletForeignSessions(anyLong(), anyList());

        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Bottom sheet never fully loaded",
                            mBottomSheetController.getCurrentSheetContent(),
                            Matchers.instanceOf(RestoreTabsPromoSheetContent.class));
                });
        Assert.assertTrue(
                mBottomSheetController.getCurrentSheetContent()
                        instanceof RestoreTabsPromoSheetContent);

        pressBack();
        verify(mMockTracker, times(1)).dismissed(eq(RESTORE_TABS_FEATURE));
    }

    @Test
    @MediumTest
    public void testRestoreTabsPromo_noSyncedDevicesNoTrigger() {
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        verify(mMockTracker, never()).shouldTriggerHelpUI(eq(RESTORE_TABS_FEATURE));
        Assert.assertFalse(mBottomSheetController.isSheetOpen());
        verify(mMockTracker, never()).dismissed(eq(RESTORE_TABS_FEATURE));
    }

    @Test
    @MediumTest
    public void testRestoreTabsPromo_testOpenDeviceScreenAndRestore() {
        TabUiTestHelper.createTabs(mActivityTestRule.getActivity(), false, 6);
        setupMultipleDevicesAndTabsMockData();
        Assert.assertEquals(6, mActivityTestRule.tabsCount(false));
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());

        // Check the latest device is selected with the proper info.
        onView(withId(R.id.restore_tabs_promo_sheet_device_icon)).check(matches(isDisplayed()));
        onView(withId(R.id.restore_tabs_promo_sheet_device_name))
                .check(matches(withText("John's iPad Air")));
        onView(withId(R.id.restore_tabs_button_open_tabs)).check(matches(withText("Open 3 tabs")));

        // Clicking on it opens the device sheet.
        onView(withId(R.id.restore_tabs_selected_device_view)).perform(click());
        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(
                                                R.string.restore_tabs_device_screen_sheet_title)))
                .check(matches(isDisplayed()));

        // Clicking on another device opens the promo sheet again.
        onView(withText("John's iPhone 6")).check(matches(isDisplayed()));
        onView(withText("John's iPhone 6")).perform(click());
        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(R.string.restore_tabs_promo_sheet_title)))
                .check(matches(isDisplayed()));
        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(
                                                R.string
                                                        .restore_tabs_promo_sheet_subtitle_multi_device)))
                .check(matches(isDisplayed()));

        // Accept the bottom sheet.
        onView(withId(R.id.restore_tabs_button_open_tabs)).check(matches(withText("Open 1 tab")));
        onView(withId(R.id.restore_tabs_button_open_tabs)).perform(click());
        Assert.assertEquals(7, mActivityTestRule.tabsCount(false));
        Assert.assertFalse(mBottomSheetController.isSheetOpen());

        int tabSwitcherAncestorViewId =
                TabUiTestHelper.getTabSwitcherAncestorId(mActivityTestRule.getActivity());
        // Make sure the grid tab switcher is scrolled down to show the selected tab.
        onView(
                        allOf(
                                withId(org.chromium.chrome.test.R.id.tab_list_recycler_view),
                                isDescendantOfA(withId(tabSwitcherAncestorViewId))))
                .check(
                        (v, noMatchException) -> {
                            if (noMatchException != null) throw noMatchException;
                            Assert.assertTrue(v instanceof RecyclerView);
                            LinearLayoutManager layoutManager =
                                    (LinearLayoutManager) ((RecyclerView) v).getLayoutManager();
                            Assert.assertEquals(
                                    4, layoutManager.findFirstCompletelyVisibleItemPosition());
                        });
    }

    @Test
    @MediumTest
    public void testRestoreTabsPromo_testOpenReviewTabsScreenBackButtonRestore() {
        setupMultipleDevicesAndTabsMockData();
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());

        onView(withId(R.id.restore_tabs_button_open_tabs)).check(matches(withText("Open 3 tabs")));

        // Clicking on the review tabs button.
        onView(withId(R.id.restore_tabs_button_review_tabs)).perform(click());
        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(
                                                R.string
                                                        .restore_tabs_review_tabs_screen_sheet_title)))
                .check(matches(isDisplayed()));

        // Deselect a tab.
        onView(withId(R.id.restore_tabs_button_open_selected_tabs))
                .check(matches(withText("Open 3 tabs")));
        onView(withId(R.id.restore_tabs_button_change_all_tabs_selection))
                .check(matches(withText("Deselect all")));
        onView(withText("title2")).perform(click());
        onView(withId(R.id.restore_tabs_button_open_selected_tabs))
                .check(matches(withText("Open 2 tabs")));
        onView(withId(R.id.restore_tabs_button_change_all_tabs_selection))
                .check(matches(withText("Select all")));

        // Clicking on the back button opens the promo sheet again.
        onView(withId(R.id.restore_tabs_toolbar_back_image_button)).perform(click());
        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(R.string.restore_tabs_promo_sheet_title)))
                .check(matches(isDisplayed()));
        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(
                                                R.string
                                                        .restore_tabs_promo_sheet_subtitle_multi_device)))
                .check(matches(isDisplayed()));

        // Check that tab selection state for the selected device is persistent.
        onView(withId(R.id.restore_tabs_button_open_tabs)).check(matches(withText("Open 2 tabs")));

        pressBack();
        Assert.assertFalse(mBottomSheetController.isSheetOpen());
    }

    @Test
    @MediumTest
    public void testRestoreTabsPromo_testOpenReviewTabsScreenBackPressChangeDevice() {
        setupMultipleDevicesAndTabsMockData();
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());

        onView(withId(R.id.restore_tabs_button_open_tabs)).check(matches(withText("Open 3 tabs")));

        // Clicking on the review tabs button.
        onView(withId(R.id.restore_tabs_button_review_tabs)).perform(click());
        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(
                                                R.string
                                                        .restore_tabs_review_tabs_screen_sheet_title)))
                .check(matches(isDisplayed()));

        // Ensure tabs exist
        onView(withText("title2")).check(matches(isDisplayed()));
        onView(withText("title3")).check(matches(isDisplayed()));
        onView(withText("title4")).check(matches(isDisplayed()));

        // Deselect a tab.
        onView(withId(R.id.restore_tabs_button_open_selected_tabs))
                .check(matches(withText("Open 3 tabs")));
        onView(withId(R.id.restore_tabs_button_change_all_tabs_selection))
                .check(matches(withText("Deselect all")));
        onView(withText("title2")).perform(click());
        onView(withId(R.id.restore_tabs_button_open_selected_tabs))
                .check(matches(withText("Open 2 tabs")));
        onView(withId(R.id.restore_tabs_button_change_all_tabs_selection))
                .check(matches(withText("Select all")));

        // Clicking on the back button opens the promo sheet again.
        onView(withId(R.id.restore_tabs_toolbar_back_image_button)).perform(click());
        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(R.string.restore_tabs_promo_sheet_title)))
                .check(matches(isDisplayed()));
        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(
                                                R.string
                                                        .restore_tabs_promo_sheet_subtitle_multi_device)))
                .check(matches(isDisplayed()));

        // Check that tab selection state for the selected device is persistent.
        onView(withId(R.id.restore_tabs_button_open_tabs)).check(matches(withText("Open 2 tabs")));

        // Enter the device screen and select another device.
        onView(withId(R.id.restore_tabs_selected_device_view)).perform(click());
        onView(withText("John's iPhone 6")).perform(click());

        // Re-enter the review tabs screen to make sure new tabs are shown.
        onView(withId(R.id.restore_tabs_button_review_tabs)).perform(click());
        onView(withText("title1")).check(matches(isDisplayed()));
        pressBack();

        // Exit and ensure the tab count is reset.
        onView(withId(R.id.restore_tabs_button_open_tabs)).check(matches(withText("Open 1 tab")));

        pressBack();
        Assert.assertFalse(mBottomSheetController.isSheetOpen());
    }

    @Test
    @MediumTest
    public void testRestoreTabsPromo_testReviewTabsScreenToggleSelection() {
        setupMultipleDevicesAndTabsMockData();
        Assert.assertEquals(1, mActivityTestRule.tabsCount(false));
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());

        onView(withId(R.id.restore_tabs_button_open_tabs)).check(matches(withText("Open 3 tabs")));

        // Clicking on the review tabs button.
        onView(withId(R.id.restore_tabs_button_review_tabs)).perform(click());
        onView(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getString(
                                                R.string
                                                        .restore_tabs_review_tabs_screen_sheet_title)))
                .check(matches(isDisplayed()));

        // Deselect all and select all.
        onView(withId(R.id.restore_tabs_button_change_all_tabs_selection))
                .check(matches(withText("Deselect all")));
        onView(withId(R.id.restore_tabs_button_open_selected_tabs))
                .check(matches(withText("Open 3 tabs")));
        onView(withId(R.id.restore_tabs_button_change_all_tabs_selection)).perform(click());
        onView(withId(R.id.restore_tabs_button_change_all_tabs_selection))
                .check(matches(withText("Select all")));
        onView(withId(R.id.restore_tabs_button_open_selected_tabs))
                .check(matches(withText("Open 0 tabs")));
        onView(withId(R.id.restore_tabs_button_change_all_tabs_selection)).perform(click());
        onView(withId(R.id.restore_tabs_button_change_all_tabs_selection))
                .check(matches(withText("Deselect all")));
        onView(withId(R.id.restore_tabs_button_open_selected_tabs))
                .check(matches(withText("Open 3 tabs")));
        onView(withText("title2")).perform(click());
        onView(withText("title3")).perform(click());

        // Restore from review tabs screen.
        onView(withId(R.id.restore_tabs_button_open_selected_tabs))
                .check(matches(withText("Open 1 tab")));
        onView(withId(R.id.restore_tabs_button_open_selected_tabs)).perform(click());
        Assert.assertEquals(2, mActivityTestRule.tabsCount(false));
        Assert.assertFalse(mBottomSheetController.isSheetOpen());
    }

    private void setupMultipleDevicesAndTabsMockData() {
        // Setup mock data
        ForeignSessionTab tab1 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title1", 31L, 31L, 0);
        List<ForeignSessionTab> tabs1 = new ArrayList<>();
        tabs1.add(tab1);
        ForeignSessionWindow window1 = new ForeignSessionWindow(32L, 1, tabs1);
        List<ForeignSessionWindow> windows1 = new ArrayList<>();
        windows1.add(window1);
        ForeignSession session1 =
                new ForeignSession("tag", "John's iPhone 6", 33L, windows1, FormFactor.PHONE);

        ForeignSessionTab tab2 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title2", 34L, 34L, 0);
        ForeignSessionTab tab3 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title3", 35L, 35L, 0);
        ForeignSessionTab tab4 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title4", 36L, 36L, 0);
        List<ForeignSessionTab> tabs2 = new ArrayList<>();
        tabs2.add(tab2);
        tabs2.add(tab3);
        tabs2.add(tab4);
        ForeignSessionWindow window2 = new ForeignSessionWindow(37L, 1, tabs2);
        List<ForeignSessionWindow> windows2 = new ArrayList<>();
        windows2.add(window2);
        ForeignSession session2 =
                new ForeignSession("tag", "John's iPad Air", 38L, windows2, FormFactor.TABLET);
        List<ForeignSession> sessions = new ArrayList<>();
        sessions.add(session1);
        sessions.add(session2);

        doReturn(true).when(mMockTracker).wouldTriggerHelpUI(eq(RESTORE_TABS_FEATURE));
        doReturn(true).when(mMockTracker).shouldTriggerHelpUI(eq(RESTORE_TABS_FEATURE));
        doAnswer(
                        invocation -> {
                            List<ForeignSession> invoked_sessions = invocation.getArgument(1);
                            invoked_sessions.addAll(sessions);
                            return true;
                        })
                .when(mForeignSessionHelperJniSpy)
                .getMobileAndTabletForeignSessions(anyLong(), anyList());
    }
}
