// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.content.res.Configuration.ORIENTATION_LANDSCAPE;
import static android.content.res.Configuration.ORIENTATION_PORTRAIT;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.RecyclerViewActions.scrollToPosition;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.espresso.Espresso;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.ChromeTabbedActivity;
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
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.hub.ArchivedTabsDialogStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TriggerSource;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** End-to-end test for ArchivedTabsDialogCoordinator. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "TODO(crbug.com/348068134): Batch this test suite.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures("IPH_AndroidTabDeclutter")
public class ArchivedTabsDialogCoordinatorTest {
    private static final int TAB1_ID = 456;
    private static final Token TAB_GROUP_ID1 = new Token(829L, 283L);
    private static final String SYNC_GROUP_ID1 = "test_sync_group_id1";
    private static final String GROUP_TITLE1 = "My Group";
    private static final @TabGroupColorId int SYNC_GROUP_COLOR1 = TabGroupColorId.BLUE;
    private static final GURL TAB_URL_1 = new GURL("https://url1.com");

    @Rule
    public FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(1)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_HUB)
                    .build();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupSyncService mTabGroupSyncService;

    @Captor ArgumentCaptor<TabGroupSyncService.Observer> mTabGroupSyncServiceObserverCaptor;

    private final TabListEditorTestingRobot mRobot = new TabListEditorTestingRobot();

    private Profile mProfile;
    private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private TabModel mArchivedTabModel;
    private TabCreator mRegularTabCreator;
    private TabModel mRegularTabModel;
    private UserActionTester mUserActionTester;
    private TabArchiveSettings mTabArchiveSettings;
    private int mTimesShown;

    private WebPageStation mInitialPage;

    @Before
    public void setUp() throws Exception {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        doNothing()
                .when(mTabGroupSyncService)
                .addObserver(mTabGroupSyncServiceObserverCaptor.capture());

        mInitialPage = mCtaTestRule.startOnBlankPage();
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile = cta.getProfileProviderSupplier().get().getOriginalProfile();
                    mRegularTabCreator = cta.getTabCreator(false);
                    mRegularTabModel = cta.getTabModelSelectorSupplier().get().getModel(false);
                });

        mArchivedTabModelOrchestrator = ArchivedTabModelOrchestrator.getForProfile(mProfile);
        mArchivedTabModel = mArchivedTabModelOrchestrator.getTabModelSelector().getModel(false);
        mUserActionTester = new UserActionTester();
        mTabArchiveSettings = mArchivedTabModelOrchestrator.getTabArchiveSettings();
        mTabArchiveSettings.setShouldShowDialogIphForTesting(false);
        waitForArchivedTabModelsToLoad(mArchivedTabModelOrchestrator);
    }

    @After
    public void tearDown() {
        mUserActionTester.tearDown();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mArchivedTabModel
                            .getTabRemover()
                            .forceCloseTabs(TabClosureParams.closeAllTabs().build());
                    mTabArchiveSettings.resetSettingsForTesting();
                });
    }

    @Test
    @MediumTest
    public void testOneInactiveTab() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

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

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("2 inactive tabs")).check(matches(isDisplayed()));
        // The dialog isn't scrollable, so the shadow should be hidden.
        onView(withId(R.id.close_all_tabs_button_container_shadow))
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testDialogIph() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        mTabArchiveSettings.setShouldShowDialogIphForTesting(true);

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("2 inactive tabs")).check(matches(isDisplayed()));
        mRobot.resultRobot.verifyAdapterHasItemCount(3);
        assertEquals(1, mUserActionTester.getActionCount("Tabs.ArchivedTabsDialogIphShown"));
    }

    @Test
    @MediumTest
    public void testDialogIph_Clicked() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        mTabArchiveSettings.setShouldShowDialogIphForTesting(true);

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

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
    @LargeTest
    public void testDialogIph_CloseDialog() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        mTabArchiveSettings.setShouldShowDialogIphForTesting(true);

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        ArchivedTabsDialogStation archivedTabsDialogStation =
                tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        dismissIphMessage(/* numOfArchivedTabs= */ 2);
        assertTrue(mTabArchiveSettings.shouldShowDialogIph());
        assertEquals(1, mUserActionTester.getActionCount("Tabs.ArchivedTabsDialogIphDismissed"));
        tabSwitcherStation = archivedTabsDialogStation.closeDialog();
        mRobot.resultRobot.verifyTabListEditorIsHidden();

        archivedTabsDialogStation =
                tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        dismissIphMessage(/* numOfArchivedTabs= */ 2);
        assertTrue(mTabArchiveSettings.shouldShowDialogIph());
        assertEquals(2, mUserActionTester.getActionCount("Tabs.ArchivedTabsDialogIphDismissed"));
        tabSwitcherStation = archivedTabsDialogStation.closeDialog();
        mRobot.resultRobot.verifyTabListEditorIsHidden();

        archivedTabsDialogStation =
                tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        dismissIphMessage(/* numOfArchivedTabs= */ 2);
        assertFalse(mTabArchiveSettings.shouldShowDialogIph());
        assertEquals(3, mUserActionTester.getActionCount("Tabs.ArchivedTabsDialogIphDismissed"));
        tabSwitcherStation = archivedTabsDialogStation.closeDialog();
        mRobot.resultRobot.verifyTabListEditorIsHidden();

        // After 3 dismisses, the iph message won't show again.
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();
        mRobot.resultRobot.verifyAdapterHasItemCount(2);
    }

    @Test
    @MediumTest
    public void testRestoreAllInactiveTabs() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

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
    public void testRestoreArchivedTabsAndOpenLast() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("2 inactive tabs")).check(matches(isDisplayed()));

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Restore all");
        mRobot.resultRobot.verifyTabListEditorIsHidden();

        // Back to the regular tab switcher -- verify that the undo button is showing.
        int index = 2;
        onView(withId(R.id.tab_list_recycler_view))
                .perform(
                        new ViewAction() {
                            @Override
                            public Matcher<View> getConstraints() {
                                return isDisplayed();
                            }

                            @Override
                            public String getDescription() {
                                return "click on end button of item with index "
                                        + String.valueOf(index);
                            }

                            @Override
                            public void perform(UiController uiController, View view) {
                                RecyclerView recyclerView = (RecyclerView) view;
                                RecyclerView.ViewHolder viewHolder =
                                        recyclerView.findViewHolderForAdapterPosition(index);
                                if (viewHolder.itemView == null) return;
                                viewHolder.itemView.performClick();
                            }
                        });
        LayoutTestUtils.waitForLayout(
                mCtaTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING);
        Tab activityTab = mCtaTestRule.getActivity().getActivityTabProvider().get();
        assertEquals(mRegularTabModel.getTabAt(2), activityTab);
    }

    @Test
    @MediumTest
    public void testSettings() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

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
    public void testTurnOffArchiveThroughSettings() throws Exception {
        mTabArchiveSettings.setShouldShowDialogIphForTesting(true);
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        ArchivedTabsDialogStation archivedTabsDialogStation =
                tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        archivedTabsDialogStation.openSettings(
                () -> {
                    mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Settings");
                });
        mRobot.resultRobot.verifyTabListEditorIsHidden();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.OpenArchivedTabsSettingsMenuItem"));

        mArchivedTabModelOrchestrator.resetRescueArchivedTabsForTesting();
        onView(withText("Never")).perform(click());

        CriteriaHelper.pollUiThread(() -> mRegularTabModel.getCount() == 3);
        assertEquals(0, mArchivedTabModel.getCount());
    }

    @Test
    @MediumTest
    public void testCloseAllArchivedTabs() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher("Tabs.CloseAllArchivedTabs.TabCount", 2);

        onView(withText("2 inactive tabs")).check(matches(isDisplayed()));
        onView(withText("Close all inactive tabs")).perform(click());
        onView(withText("Close all")).perform(click());

        mRobot.resultRobot.verifyTabListEditorIsHidden();
        assertEquals(0, mArchivedTabModel.getCount());
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.CloseAllArchivedTabsMenuItem"));
    }

    @Test
    @MediumTest
    public void testCloseAllArchivedTabs_Cancel() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("2 inactive tabs")).check(matches(isDisplayed()));
        onView(withText("Close all inactive tabs")).perform(click());
        onView(withText("Cancel")).perform(click());

        assertEquals(2, mArchivedTabModel.getCount());
        assertEquals(0, mUserActionTester.getActionCount("Tabs.CloseAllArchivedTabsMenuItem"));
    }

    @Test
    @MediumTest
    public void testSelectTabs() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

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

        mRobot.actionRobot.clickToolbarNavigationButton(
                R.string.accessibility_archived_tabs_dialog_back_button);
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

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

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

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

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
        mRobot.resultRobot
                .verifyAdapterHasItemCount(1)
                .verifyUndoSnackbarWithTextIsShown("2 tabs closed");
        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.CloseArchivedTabsMenuItem"));
    }

    @Test
    @MediumTest
    public void testSelectionModeMenuItem_CloseTabs_SelectAll() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");
        addArchivedTab(new GURL("https://google.com"), "test 3");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(3, mArchivedTabModel.getCount());

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select tabs");

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.CloseArchivedTabsMenuItem.TabCount", 3);
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.actionRobot.clickItemAtAdapterPosition(2);
        mRobot.resultRobot.verifyToolbarSelectionText("3 tabs");
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Close tabs");
        mRobot.resultRobot.verifyUndoSnackbarWithTextIsShown("3 tabs closed");

        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.CloseArchivedTabsMenuItem"));
    }

    @Test
    @MediumTest
    public void testSelectionModeMenuItem_RestoreTabs() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");
        addArchivedTab(new GURL("https://google.com"), "test 3");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

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

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCtaTestRule.getActivity().getOnBackPressedDispatcher().onBackPressed();
                });
        mRobot.resultRobot.verifyTabListEditorIsHidden();
        assertNull(mCtaTestRule.getActivity().findViewById(R.id.archived_tabs_dialog));
    }

    @Test
    @MediumTest
    public void testRestoreAndOpenSingleTab() throws Exception {
        GURL archivedUrl = new GURL("https://www.google.com");
        Tab tab = addArchivedTab(archivedUrl, "test 1");
        int tabId = tab.getId();

        addArchivedTab(new GURL("https://test.com"), "test 2");
        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(2, mArchivedTabModel.getCount());

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyTabListEditorIsHidden();
        assertEquals(2, mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());

        LayoutTestUtils.waitForLayout(
                mCtaTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING);
        Tab activityTab = mCtaTestRule.getActivity().getActivityTabProvider().get();
        CriteriaHelper.pollUiThread(() -> activityTab.getId() == tabId);
        assertEquals(1, mUserActionTester.getActionCount("Tabs.RestoreSingleTab"));
    }

    @Test
    @MediumTest
    public void testCloseArchivedTab() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("2 inactive tabs")).check(matches(isDisplayed()));

        mRobot.actionRobot.clickViewIdAtAdapterPosition(1, R.id.action_button);
        mRobot.resultRobot
                .verifyAdapterHasItemCount(1)
                .verifyUndoSnackbarWithTextIsShown("Closed google");
    }

    @Test
    @MediumTest
    public void testCloseArchivedTab_SnackbarResetForTabSwitcher() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("2 inactive tabs")).check(matches(isDisplayed()));

        mRobot.actionRobot.clickViewIdAtAdapterPosition(1, R.id.action_button);
        mRobot.resultRobot
                .verifyAdapterHasItemCount(1)
                .verifyUndoSnackbarWithTextIsShown("Closed google");
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mCtaTestRule.getActivity().getOnBackPressedDispatcher().onBackPressed();
                });
        mRobot.resultRobot.verifyTabListEditorIsHidden();

        // Back to the regular tab switcher -- verify that the undo button is showing.
        int index = 1;
        onView(withId(R.id.tab_list_recycler_view))
                .perform(
                        new ViewAction() {
                            @Override
                            public Matcher<View> getConstraints() {
                                return isDisplayed();
                            }

                            @Override
                            public String getDescription() {
                                return "click on end button of item with index "
                                        + String.valueOf(index);
                            }

                            @Override
                            public void perform(UiController uiController, View view) {
                                RecyclerView recyclerView = (RecyclerView) view;
                                RecyclerView.ViewHolder viewHolder =
                                        recyclerView.findViewHolderForAdapterPosition(index);
                                if (viewHolder.itemView == null) return;

                                viewHolder.itemView.findViewById(R.id.action_button).performClick();
                            }
                        });
        mRobot.resultRobot.verifyUndoSnackbarWithTextIsShown("Closed about:blank");
    }

    @Test
    @LargeTest
    public void testContentDescription() {
        onView(withContentDescription(R.string.accessibility_tab_selection_editor_back_button));
        onView(withContentDescription(R.string.accessibility_tab_selection_editor));
    }

    @Test
    @MediumTest
    public void testBottomShadowView() throws Exception {
        for (int i = 0; i < 50; i++) {
            addArchivedTab(new GURL("https://google.com?q=" + i), "test " + i);
        }

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("50 inactive tabs")).check(matches(isDisplayed()));
        mRobot.resultRobot.verifyTabListEditorIsVisible().verifyAdapterHasItemCount(50);

        // When there is more than a page of tabs, then the bottom container should have a shadow.
        onView(withId(R.id.close_all_tabs_button_container_shadow)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    // Scrolling all the way down on tablets is flaky.
    @Restriction(DeviceFormFactor.PHONE)
    public void testBottomShadowView_DisappersWhenFullyScrolled() throws Exception {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");
        addArchivedTab(new GURL("https://google.com"), "test 3");
        addArchivedTab(new GURL("https://google.com"), "test 4");
        addArchivedTab(new GURL("https://google.com"), "test 5");
        addArchivedTab(new GURL("https://google.com"), "test 6");
        addArchivedTab(new GURL("https://google.com"), "test 7");
        addArchivedTab(new GURL("https://google.com"), "test 8");
        addArchivedTab(new GURL("https://google.com"), "test 9");
        addArchivedTab(new GURL("https://google.com"), "test 10");
        addArchivedTab(new GURL("https://google.com"), "test 11");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("11 inactive tabs")).check(matches(isDisplayed()));
        mRobot.resultRobot.verifyTabListEditorIsVisible().verifyAdapterHasItemCount(11);

        // When there is more than a page of tabs, then the bottom container should have a shadow.

        onView(withId(R.id.close_all_tabs_button_container_shadow)).check(matches(isDisplayed()));

        // When the recycler view is scrolled all the way down, the shadow should be hidden.
        onView(
                        allOf(
                                isDescendantOfA(withId(R.id.tab_list_editor_container)),
                                withId(R.id.tab_list_recycler_view)))
                .perform(scrollToPosition(10));
        onView(withId(R.id.close_all_tabs_button_container_shadow))
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.TABLET})
    @Feature({"RenderTest"})
    public void testMessageResizedOnTablet() throws Exception {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        ActivityTestUtils.rotateActivityToOrientation(cta, ORIENTATION_PORTRAIT);
        addArchivedTab(new GURL("https://www.google.com/"), "test 2");

        TabUiTestHelper.enterTabSwitcher(mCtaTestRule.getActivity());
        mRenderTestRule.render(
                cta.findViewById(R.id.pane_frame), "archived_tabs_message_tablet_portrait");

        ActivityTestUtils.rotateActivityToOrientation(cta, ORIENTATION_LANDSCAPE);
        mRenderTestRule.render(
                cta.findViewById(R.id.pane_frame), "archived_tabs_message_tablet_landscape");

        ActivityTestUtils.clearActivityOrientation(cta);
    }

    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.TABLET})
    @Feature({"RenderTest"})
    public void testIphMessageResizedOnTablet() throws Exception {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        ActivityTestUtils.rotateActivityToOrientation(cta, ORIENTATION_PORTRAIT);
        addArchivedTab(new GURL("https://www.google1.com/"), "test 1");
        addArchivedTab(new GURL("https://www.google2.com/"), "test 2");
        addArchivedTab(new GURL("https://www.google3.com/"), "test 3");
        addArchivedTab(new GURL("https://www.google4.com/"), "test 4");
        mTabArchiveSettings.setShouldShowDialogIphForTesting(true);

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        mRenderTestRule.render(
                cta.findViewById(R.id.archived_tabs_dialog),
                "archived_tabs_iph_message_tablet_portrait");
        ActivityTestUtils.rotateActivityToOrientation(cta, ORIENTATION_LANDSCAPE);
        mRenderTestRule.render(
                cta.findViewById(R.id.archived_tabs_dialog),
                "archived_tabs_iph_message_tablet_landscape");
        ActivityTestUtils.clearActivityOrientation(cta);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
    @DisabledTest(message = "crbug.com/417674987")
    public void testCloseAllArchivedTabs_WithSyncedTabGroups() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        SavedTabGroup savedTabGroup =
                createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 1, true);
        addArchivedTab(new GURL("https://google.com"), "test 1");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher("Tabs.CloseAllArchivedTabs.TabCount", 1);

        onView(withText("2 inactive tabs")).check(matches(isDisplayed()));
        onView(withText("Close all inactive tabs")).perform(click());
        onView(withText("Close all")).perform(click());

        // Assert that the group was archived and emit an event with the new archive status.
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID1, false);
        savedTabGroup.archivalTimeMs = null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupSyncServiceObserverCaptor
                            .getAllValues()
                            .get(1)
                            .onTabGroupUpdated(savedTabGroup, TriggerSource.REMOTE);
                });

        mRobot.resultRobot.verifyTabListEditorIsHidden();
        assertEquals(0, mArchivedTabModel.getCount());
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.CloseAllArchivedTabsMenuItem"));
        assertNull(mCtaTestRule.getActivity().findViewById(R.id.archived_tabs_dialog));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
    @DisabledTest(message = "crbug.com/417674987")
    public void testSelectCloseArchivedTabs_WithSyncedTabGroups() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        SavedTabGroup savedTabGroup =
                createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 1, true);
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");
        addArchivedTab(new GURL("https://google.com"), "test 3");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(3, mArchivedTabModel.getCount());

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select tabs");

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.CloseArchivedTabsMenuItem.TabCount", 2);
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.actionRobot.clickItemAtAdapterPosition(2);
        mRobot.resultRobot.verifyToolbarSelectionText("3 tabs");
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Close tabs");

        // Assert that the group was archived and emit an event with the new archive status.
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID1, false);
        savedTabGroup.archivalTimeMs = null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupSyncServiceObserverCaptor
                            .getAllValues()
                            .get(1)
                            .onTabGroupUpdated(savedTabGroup, TriggerSource.REMOTE);
                });

        mRobot.resultRobot
                .verifyAdapterHasItemCount(1)
                .verifyUndoSnackbarWithTextIsShown("2 tabs closed");
        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.CloseArchivedTabsMenuItem"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
    @DisabledTest(message = "crbug.com/417674987")
    public void testSelectAllCloseArchivedTabs_WithSyncedTabGroups() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        SavedTabGroup savedTabGroup =
                createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 1, true);
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(2, mArchivedTabModel.getCount());

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select tabs");

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.CloseArchivedTabsMenuItem.TabCount", 2);
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.actionRobot.clickItemAtAdapterPosition(2);
        mRobot.resultRobot.verifyToolbarSelectionText("3 tabs");
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Close tabs");

        // Assert that the group was archived and emit an event with the new archive status.
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID1, false);
        savedTabGroup.archivalTimeMs = null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupSyncServiceObserverCaptor
                            .getAllValues()
                            .get(1)
                            .onTabGroupUpdated(savedTabGroup, TriggerSource.REMOTE);
                });

        mRobot.resultRobot.verifyUndoSnackbarWithTextIsShown("2 tabs closed");

        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.CloseArchivedTabsMenuItem"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
    @DisabledTest(message = "crbug.com/417674987")
    public void testRestoreAllInactiveTabs_WithSyncedTabGroups() throws Exception {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        SavedTabGroup savedTabGroup =
                createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 1, true);
        addArchivedTab(new GURL("https://google.com"), "test 1");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("2 inactive tabs")).check(matches(isDisplayed()));

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.RestoreAllArchivedTabsMenuItem.TabCount", 1);
        assertEquals(1, mRegularTabModel.getCount());

        // Mock the sync backend being initialized so the tab group is restored via
        // createNewTabGroup and LocalTabGroupMutationHelper, reflected in the regular tab model.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupSyncServiceObserverCaptor.getAllValues().get(0).onInitialized();
                });

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Restore all");

        // Assert that the group was unarchived and emit an event with the new archive status.
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID1, false);
        savedTabGroup.archivalTimeMs = null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupSyncServiceObserverCaptor
                            .getAllValues()
                            .get(1)
                            .onTabGroupUpdated(savedTabGroup, TriggerSource.REMOTE);
                });

        mRobot.resultRobot.verifyTabListEditorIsHidden();
        // This count includes the restored tab group.
        assertEquals(3, mRegularTabModel.getCount());
        assertEquals(0, mArchivedTabModel.getCount());
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.RestoreAllArchivedTabsMenuItem"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
    @DisabledTest(message = "crbug.com/417674987")
    public void testSelectionModeMenuItem_RestoreTabGroups() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        SavedTabGroup savedTabGroup =
                createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 1, true);
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        assertEquals(1, mRegularTabModel.getCount());
        assertEquals(2, mArchivedTabModel.getCount());

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select tabs");

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.RestoreArchivedTabsMenuItem.TabCount", 1);
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarSelectionText("2 tabs");

        // Mock the sync backend being initialized so the tab group is restored via
        // createNewTabGroup and LocalTabGroupMutationHelper, reflected in the regular tab model.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupSyncServiceObserverCaptor.getAllValues().get(0).onInitialized();
                });

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Restore tabs");

        // Assert that the group was unarchived and emit an event with the new archive status.
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID1, false);
        savedTabGroup.archivalTimeMs = null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabGroupSyncServiceObserverCaptor
                            .getAllValues()
                            .get(1)
                            .onTabGroupUpdated(savedTabGroup, TriggerSource.REMOTE);
                });

        mRobot.resultRobot.verifyAdapterHasItemCount(1);
        // This count includes the restored tab group.
        assertEquals(3, mRegularTabModel.getCount());
        assertEquals(1, mArchivedTabModel.getCount());
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.RestoreArchivedTabsMenuItem"));
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

    private SavedTabGroup createSavedTabGroup(
            String syncId,
            String title,
            @TabGroupColorId int color,
            int tabCount,
            boolean isArchived) {
        SavedTabGroupTab savedTab = new SavedTabGroupTab();
        savedTab.url = TAB_URL_1;
        savedTab.syncId = syncId;
        List<SavedTabGroupTab> savedTabs = new ArrayList<>(Collections.nCopies(tabCount, savedTab));
        SavedTabGroup savedTabGroup = new SavedTabGroup();
        savedTabGroup.syncId = syncId;
        savedTabGroup.savedTabs = savedTabs;
        savedTabGroup.title = title;
        savedTabGroup.color = color;
        savedTabGroup.archivalTimeMs = isArchived ? System.currentTimeMillis() : null;

        when(mTabGroupSyncService.getGroup(syncId)).thenReturn(savedTabGroup);

        return savedTabGroup;
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

    private void dismissIphMessage(int numOfArchivedTabs) {
        mRobot.resultRobot.verifyAdapterHasItemCount(3);
        mRobot.actionRobot.clickViewIdAtAdapterPosition(0, R.id.close_button);
        mRobot.resultRobot.verifyAdapterHasItemCount(2);
    }
}
