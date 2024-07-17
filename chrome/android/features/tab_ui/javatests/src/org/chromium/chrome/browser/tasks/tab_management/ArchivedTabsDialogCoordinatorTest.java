// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.ViewGroup;

import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator.Observer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.layouts.LayoutTestUtils;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.GURL;

/** End-to-end test for ArchivedTabsDialogCoordinator. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "TODO(crbug.com/348068134): Batch this test suite.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER})
public class ArchivedTabsDialogCoordinatorTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private final TabListEditorTestingRobot mRobot = new TabListEditorTestingRobot();

    private Profile mProfile;
    private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private TabModel mArchivedTabModel;
    private ViewGroup mParentView;
    private TabCreator mRegularTabCreator;
    private TabModel mRegularTabModel;
    private UserActionTester mUserActionTester;
    private TabArchiveSettings mTabArchiveSettings;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile =
                            mActivityTestRule
                                    .getActivity()
                                    .getProfileProviderSupplier()
                                    .get()
                                    .getOriginalProfile();
                    mRegularTabCreator = mActivityTestRule.getActivity().getTabCreator(false);
                    mRegularTabModel =
                            mActivityTestRule
                                    .getActivity()
                                    .getTabModelSelectorSupplier()
                                    .get()
                                    .getModel(false);
                });

        mArchivedTabModelOrchestrator = ArchivedTabModelOrchestrator.getForProfile(mProfile);
        mArchivedTabModel = mArchivedTabModelOrchestrator.getTabModelSelector().getModel(false);
        mUserActionTester = new UserActionTester();
        mTabArchiveSettings = mArchivedTabModelOrchestrator.getTabArchiveSettings();
        mTabArchiveSettings.setShouldShowDialogIph(false);
        waitForArchivedTabModelsToLoad(mArchivedTabModelOrchestrator);
    }

    @After
    public void tearDown() {
        mUserActionTester.tearDown();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mArchivedTabModel.closeAllTabs();
                    mTabArchiveSettings.resetSettingsForTesting();
                });
    }

    @Test
    @MediumTest
    public void testOneInactiveTab() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 2");
        showDialog(1);
        onView(withText("1 inactive tab")).check(matches(isDisplayed()));
        mRobot.resultRobot.verifyTabListEditorIsVisible().verifyAdapterHasItemCount(1);

        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot
                .verifyToolbarMenuItemState("Restore all", true)
                .verifyToolbarMenuItemState("Select tabs", true)
                .verifyToolbarMenuItemState("Settings", true);
    }

    @Test
    @MediumTest
    public void testTwoInactiveTabs() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");
        showDialog(2);
        onView(withText("2 inactive tabs")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testDialogIPH() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        mTabArchiveSettings.setShouldShowDialogIph(true);
        showDialog(2);
        onView(withText("2 inactive tabs")).check(matches(isDisplayed()));
        mRobot.resultRobot.verifyAdapterHasItemCount(3);
        assertEquals(1, mUserActionTester.getActionCount("Tabs.ArchivedTabsDialogIphShown"));
    }

    @Test
    @MediumTest
    public void testDialogIPH_Clicked() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        mTabArchiveSettings.setShouldShowDialogIph(true);
        showDialog(2);
        onView(withText("2 inactive tabs")).check(matches(isDisplayed()));

        SettingsActivity activity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SettingsActivity.class,
                        new Runnable() {
                            @Override
                            public void run() {
                                mRobot.actionRobot.clickItemAtAdapterPosition(0);
                            }
                        });
        mRobot.resultRobot.verifyTabListEditorIsHidden();
        ActivityTestUtils.waitForFragmentToAttach(activity, TabArchiveSettingsFragment.class);
        assertEquals(1, mUserActionTester.getActionCount("Tabs.ArchivedTabsDialogIphClicked"));
    }

    @Test
    @MediumTest
    public void testDialogIPH_CloseDialog() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        mTabArchiveSettings.setShouldShowDialogIph(true);
        showDialog(2);
        onView(withText("2 inactive tabs")).check(matches(isDisplayed()));
        mRobot.resultRobot.verifyAdapterHasItemCount(3);

        mRobot.actionRobot.clickViewIdAtAdapterPosition(0, R.id.close_button);
        mRobot.resultRobot.verifyAdapterHasItemCount(2);
        assertFalse(mTabArchiveSettings.shouldShowDialogIph());
        assertEquals(1, mUserActionTester.getActionCount("Tabs.ArchivedTabsDialogIphDismissed"));
    }

    @Test
    @MediumTest
    public void testRestoreAllInactiveTabs() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");
        showDialog(2);
        onView(withText("2 inactive tabs")).check(matches(isDisplayed()));

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.RestoreAllArchivedTabsMenuItem.TabCount", 2);
        assertEquals(1, mRegularTabModel.getCount());
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Restore all");
        mRobot.resultRobot.verifyTabListEditorIsHidden();
        assertEquals(3, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.RestoreAllArchivedTabsMenuItem"));
    }

    @Test
    @MediumTest
    public void testSettings() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");
        showDialog(2);

        SettingsActivity activity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SettingsActivity.class,
                        new Runnable() {
                            @Override
                            public void run() {
                                mRobot.actionRobot
                                        .clickToolbarMenuButton()
                                        .clickToolbarMenuItem("Settings");
                            }
                        });
        mRobot.resultRobot.verifyTabListEditorIsHidden();
        ActivityTestUtils.waitForFragmentToAttach(activity, TabArchiveSettingsFragment.class);
        assertEquals(1, mUserActionTester.getActionCount("Tabs.OpenArchivedTabsSettingsMenuItem"));
    }

    @Test
    @MediumTest
    public void testCloseAllArchivedTabs() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");
        showDialog(2);

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher("Tabs.CloseAllArchivedTabs.TabCount", 2);

        onView(withText("2 inactive tabs")).check(matches(isDisplayed()));
        onView(withText("Close all inactive tabs")).perform(click());
        mRobot.resultRobot.verifyTabListEditorIsHidden();
        assertEquals(0, mArchivedTabModel.getCount());
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.CloseAllArchivedTabsMenuItem"));
    }

    @Test
    @MediumTest
    public void testSelectTabs() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");
        showDialog(2);

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select tabs");
        assertEquals(1, mUserActionTester.getActionCount("Tabs.SelectArchivedTabsMenuItem"));

        mRobot.resultRobot
                .verifyAdapterHasItemCount(2)
                .verifyItemNotSelectedAtAdapterPosition(0)
                .verifyItemNotSelectedAtAdapterPosition(1)
                .verifyToolbarSelectionText("2 inactive tabs");

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarSelectionText("2 tabs");

        mRobot.actionRobot.clickToolbarNavigationButton();
        mRobot.resultRobot
                .verifyTabListEditorIsVisible()
                .verifyAdapterHasItemCount(2)
                .verifyToolbarSelectionText("2 inactive tabs");
    }

    @Test
    @MediumTest
    public void testSelectionModeMenuItems() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");
        showDialog(2);

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select tabs");
        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot
                .verifyToolbarMenuItemState("Close tabs", false)
                .verifyToolbarMenuItemState("Restore tabs", false);
        Espresso.pressBack();

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyToolbarSelectionText("1 tab");

        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot
                .verifyToolbarMenuItemState("Close tab", true)
                .verifyToolbarMenuItemState("Restore tab", true);
        Espresso.pressBack();

        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarSelectionText("2 tabs");

        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot
                .verifyToolbarMenuItemState("Close tabs", true)
                .verifyToolbarMenuItemState("Restore tabs", true);
    }

    @Test
    @MediumTest
    public void testSelectionModeMenuItem_CloseTabs() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");
        addArchivedTab(new GURL("https://google.com"), "test 3");
        showDialog(3);
        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(3, mArchivedTabModel.getCount());

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select tabs");

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.CloseArchivedTabsMenuItem.TabCount", 2);
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarSelectionText("2 tabs");
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Close tabs");
        mRobot.resultRobot.verifyAdapterHasItemCount(1);
        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.CloseArchivedTabsMenuItem"));

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select tabs");

        histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.CloseArchivedTabsMenuItem.TabCount", 1);
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Close tab");
        mRobot.resultRobot.verifyTabListEditorIsHidden();
        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        histogramExpectation.assertExpected();
        assertEquals(2, mUserActionTester.getActionCount("Tabs.CloseArchivedTabsMenuItem"));
    }

    @Test
    @MediumTest
    public void testSelectionModeMenuItem_RestoreTabs() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");
        addArchivedTab(new GURL("https://google.com"), "test 3");
        showDialog(3);
        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(3, mArchivedTabModel.getCount());

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select tabs");

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.RestoreArchivedTabsMenuItem.TabCount", 2);
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarSelectionText("2 tabs");
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Restore tabs");
        mRobot.resultRobot.verifyAdapterHasItemCount(1);
        assertEquals(3, mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.RestoreArchivedTabsMenuItem"));

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select tabs");

        histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.RestoreArchivedTabsMenuItem.TabCount", 1);
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Restore tab");
        mRobot.resultRobot.verifyTabListEditorIsHidden();
        assertEquals(4, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        histogramExpectation.assertExpected();
        assertEquals(2, mUserActionTester.getActionCount("Tabs.RestoreArchivedTabsMenuItem"));
    }

    @Test
    @MediumTest
    public void testCloseDialogWithBackButton() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 2");
        showDialog(1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getActivity().getOnBackPressedDispatcher().onBackPressed();
                });
        mRobot.resultRobot.verifyTabListEditorIsHidden();
    }

    @Test
    @MediumTest
    public void testRestoreAndOpenSingleTab() throws Exception {
        Tab tab = addArchivedTab(new GURL("https://www.google.com"), "test 1");
        addArchivedTab(new GURL("https://test.com"), "test 2");
        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(2, mArchivedTabModel.getCount());
        showDialog(2);

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyTabListEditorIsHidden();
        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());

        LayoutTestUtils.waitForLayout(
                mActivityTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING);
        Tab activityTab = mActivityTestRule.getActivity().getActivityTabProvider().get();
        assertEquals(tab, activityTab);
        assertEquals(1, mUserActionTester.getActionCount("Tabs.RestoreSingleTab"));
    }

    private void showDialog(int numTabs) {
        // Enter the tab switcher and click the message.
        TabUiTestHelper.enterTabSwitcher(mActivityTestRule.getActivity());
        onViewWaiting(
                        withText(
                                mActivityTestRule
                                        .getActivity()
                                        .getResources()
                                        .getQuantityString(
                                                R.plurals.archived_tab_card_title,
                                                numTabs,
                                                numTabs)))
                .perform(click());
        mRobot.resultRobot.verifyTabListEditorIsVisible();
    }

    private @TabListCoordinator.TabListMode int getMode() {
        return SysUtils.isLowEndDevice()
                ? TabListCoordinator.TabListMode.LIST
                : TabListCoordinator.TabListMode.GRID;
    }

    private Tab addArchivedTab(GURL url, String title) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mArchivedTabModelOrchestrator
                                .getArchivedTabCreatorForTesting()
                                .createNewTab(
                                        new LoadUrlParams(new GURL("https://google.com")),
                                        "google",
                                        TabLaunchType.FROM_RESTORE,
                                        null,
                                        mArchivedTabModel.getCount()));
    }

    private void removeArchivedTab(Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mArchivedTabModel.removeTab(tab);
                    return null;
                });
    }

    private void waitForArchivedTabModelsToLoad(
            ArchivedTabModelOrchestrator archivedTabModelOrchestrator) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    CallbackHelper callbackHelper = new CallbackHelper();
                    if (archivedTabModelOrchestrator.isTabModelInitialized()) {
                        callbackHelper.notifyCalled();
                    } else {
                        archivedTabModelOrchestrator.addObserver(
                                new Observer() {
                                    @Override
                                    public void onTabModelCreated(TabModel archivedTabModel) {
                                        archivedTabModelOrchestrator.removeObserver(this);
                                        callbackHelper.notifyCalled();
                                    }
                                });
                    }

                    return null;
                });
    }
}
