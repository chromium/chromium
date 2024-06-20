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

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.ViewGroup;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.SysUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator.Observer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
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

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(
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
        waitForArchivedTabModelsToLoad(mArchivedTabModelOrchestrator);
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mArchivedTabModel.closeAllTabs();
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
    public void testRestoreAllInactiveTabs() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");
        showDialog(2);
        onView(withText("2 inactive tabs")).check(matches(isDisplayed()));

        assertEquals(1, mRegularTabModel.getCount());
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Restore all");
        mRobot.resultRobot.verifyTabListEditorIsHidden();
        assertEquals(3, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
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
    }

    @Test
    @MediumTest
    public void testCloseAllInactiveTabs() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");
        showDialog(2);
        onView(withText("2 inactive tabs")).check(matches(isDisplayed()));
        onView(withText("Close all inactive tabs")).perform(click());
        mRobot.resultRobot.verifyTabListEditorIsHidden();
        assertEquals(0, mArchivedTabModel.getCount());
    }

    @Test
    @MediumTest
    public void testSelectTabs() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");
        showDialog(2);

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select tabs");

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
        return TestThreadUtils.runOnUiThreadBlockingNoException(
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
        TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> {
                    mArchivedTabModel.removeTab(tab);
                    return null;
                });
    }

    private void waitForArchivedTabModelsToLoad(
            ArchivedTabModelOrchestrator archivedTabModelOrchestrator) {
        TestThreadUtils.runOnUiThreadBlockingNoException(
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
