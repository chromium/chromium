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

import static org.chromium.chrome.test.util.ChromeTabUtils.getTabCountOnUiThread;

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
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.undo_tab_close_snackbar.SavedTabGroupUndoBarController;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.hub.ArchivedTabsDialogStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.SavedTabGroup;
import org.chromium.components.tab_group_sync.SavedTabGroupTab;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TriggerSource;
import org.chromium.components.tab_group_sync.VersioningMessageController;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** End-to-end test for ArchivedTabsDialogCoordinator. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "TODO(crbug.com/348068134): Batch this test suite.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures({"IPH_AndroidTabDeclutter", ChromeFeatureList.SETTINGS_MULTI_COLUMN})
public class ArchivedTabsDialogCoordinatorTest {
    private static final String SYNC_GROUP_ID1 = "test_sync_group_id1";
    private static final String SYNC_GROUP_ID2 = "test_sync_group_id2";
    private static final String SYNC_GROUP_ID3 = "test_sync_group_id3";
    private static final String GROUP_TITLE1 = "My Group";
    private static final String GROUP_TITLE2 = "Test";
    private static final @TabGroupColorId int SYNC_GROUP_COLOR1 = TabGroupColorId.BLUE;
    private static final GURL TAB_URL_1 = new GURL("https://url1.com");

    @Rule
    public FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(2)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_HUB)
                    .build();

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabGroupSyncService mTabGroupSyncService;
    @Mock private VersioningMessageController mVersioningMessageController;

    @Captor ArgumentCaptor<TabGroupSyncService.Observer> mTabGroupSyncServiceObserverCaptor;

    private final TabListEditorTestingRobot mRobot = new TabListEditorTestingRobot();

    private Profile mProfile;
    private ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private TabModel mArchivedTabModel;
    private TabCreator mRegularTabCreator;
    private TabModel mRegularTabModel;
    private UserActionTester mUserActionTester;
    private TabArchiveSettings mTabArchiveSettings;

    private WebPageStation mInitialPage;

    @Before
    public void setUp() {
        TabGroupSyncServiceFactory.setForTesting(mTabGroupSyncService);
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        doNothing()
                .when(mTabGroupSyncService)
                .addObserver(mTabGroupSyncServiceObserverCaptor.capture());
        when(mTabGroupSyncService.getVersioningMessageController())
                .thenReturn(mVersioningMessageController);

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
    public void testOneInactiveTab() {
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("1 inactive item")).check(matches(isDisplayed()));
        mRobot.resultRobot.verifyTabListEditorIsVisible().verifyAdapterHasItemCount(1);

        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot
                .verifyToolbarMenuItemState("Restore all", true)
                .verifyToolbarMenuItemState("Select items", true)
                .verifyToolbarMenuItemState("Settings", true);
    }

    @Test
    @MediumTest
    public void testTwoInactiveTabs() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("2 inactive items")).check(matches(isDisplayed()));
        // The dialog isn't scrollable, so the shadow should be hidden.
        onView(withId(R.id.close_all_tabs_button_container_shadow))
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testDialogIph() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        mTabArchiveSettings.setShouldShowDialogIphForTesting(true);

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("2 inactive items")).check(matches(isDisplayed()));
        mRobot.resultRobot.verifyAdapterHasItemCount(3);
        assertEquals(1, mUserActionTester.getActionCount("Tabs.ArchivedTabsDialogIphShown"));
    }

    @Test
    @MediumTest
    public void testDialogIph_Clicked() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        mTabArchiveSettings.setShouldShowDialogIphForTesting(true);

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("2 inactive items")).check(matches(isDisplayed()));

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
    public void testDialogIph_CloseDialog() {
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
    public void testRestoreAllInactiveTabs() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("2 inactive items")).check(matches(isDisplayed()));

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.RestoreAllArchivedTabsMenuItem.TabCount", 2);
        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Restore all");
        mRobot.resultRobot.verifyTabListEditorIsHidden();
        assertEquals(3, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.RestoreAllArchivedTabsMenuItem"));
    }

    @Test
    @MediumTest
    public void testRestoreArchivedTabsAndOpenLast() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("2 inactive items")).check(matches(isDisplayed()));

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
        ThreadUtils.runOnUiThreadBlocking(
                () -> assertEquals(mRegularTabModel.getTabAt(2), activityTab));
    }

    @Test
    @MediumTest
    public void testSettings() {
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
    public void testTurnOffArchiveThroughSettings() {
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

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.ArchiveSettings.TimeDeltaPreference", 0);

        mArchivedTabModelOrchestrator.resetRescueArchivedTabsForTesting();
        onView(withText("Never")).perform(click());
        histogramExpectation.assertExpected();

        CriteriaHelper.pollUiThread(() -> getTabCountOnUiThread(mRegularTabModel) == 3);
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));
    }

    @Test
    @MediumTest
    public void testCloseAllArchivedTabs() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher("Tabs.CloseAllArchivedTabs.TabCount", 2);

        onView(withText("2 inactive items")).check(matches(isDisplayed()));
        onView(withText("Close all")).perform(click());
        onView(withText("Close all")).perform(click());

        mRobot.resultRobot.verifyTabListEditorIsHidden();
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.CloseAllArchivedTabsMenuItem"));
    }

    @Test
    @MediumTest
    public void testCloseAllArchivedTabs_Cancel() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("2 inactive items")).check(matches(isDisplayed()));
        onView(withText("Close all")).perform(click());
        onView(withText("Cancel")).perform(click());

        assertEquals(2, getTabCountOnUiThread(mArchivedTabModel));
        assertEquals(0, mUserActionTester.getActionCount("Tabs.CloseAllArchivedTabsMenuItem"));
    }

    @Test
    @MediumTest
    public void testSelectTabs() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select items");
        assertEquals(1, mUserActionTester.getActionCount("Tabs.SelectArchivedTabsMenuItem"));

        mRobot.resultRobot
                .verifyAdapterHasItemCount(2)
                .verifyItemNotSelectedAtAdapterPosition(0)
                .verifyItemNotSelectedAtAdapterPosition(1)
                .verifyToolbarSelectionText("2 inactive items");

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarSelectionText("2 items");

        mRobot.actionRobot.clickToolbarNavigationButton(
                R.string.accessibility_archived_tabs_dialog_back_button);
        mRobot.resultRobot
                .verifyTabListEditorIsVisible()
                .verifyAdapterHasItemCount(2)
                .verifyToolbarSelectionText("2 inactive items");
    }

    @Test
    @MediumTest
    public void testSelectionModeMenuItems() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select items");
        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot
                .verifyToolbarMenuItemState("Close items", false)
                .verifyToolbarMenuItemState("Restore items", false);
        Espresso.pressBack();

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyToolbarSelectionText("1 item");

        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot
                .verifyToolbarMenuItemState("Close item", true)
                .verifyToolbarMenuItemState("Restore item", true);
        Espresso.pressBack();

        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarSelectionText("2 items");

        mRobot.actionRobot.clickToolbarMenuButton();
        mRobot.resultRobot
                .verifyToolbarMenuItemState("Close items", true)
                .verifyToolbarMenuItemState("Restore items", true);
    }

    @Test
    @MediumTest
    public void testSelectionModeMenuItem_CloseTabs() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");
        addArchivedTab(new GURL("https://google.com"), "test 3");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(3, getTabCountOnUiThread(mArchivedTabModel));

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select items");

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.CloseArchivedTabsMenuItem.TabCount", 2);
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarSelectionText("2 items");
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Close items");
        mRobot.resultRobot
                .verifyAdapterHasItemCount(1)
                .verifyUndoSnackbarWithTextIsShown("2 tabs closed");
        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));
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

        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(3, getTabCountOnUiThread(mArchivedTabModel));

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select items");

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.CloseArchivedTabsMenuItem.TabCount", 3);
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.actionRobot.clickItemAtAdapterPosition(2);
        mRobot.resultRobot.verifyToolbarSelectionText("3 items");
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Close items");
        mRobot.resultRobot.verifyUndoSnackbarWithTextIsShown("3 tabs closed");

        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));
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

        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(3, getTabCountOnUiThread(mArchivedTabModel));

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select items");

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.RestoreArchivedTabsMenuItem.TabCount", 2);
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarSelectionText("2 items");
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Restore items");
        mRobot.resultRobot.verifyAdapterHasItemCount(1);
        assertEquals(3, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.RestoreArchivedTabsMenuItem"));

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select items");

        histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "Tabs.RestoreArchivedTabsMenuItem.TabCount", 1);
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Restore item");
        mRobot.resultRobot.verifyTabListEditorIsHidden();
        assertEquals(4, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));
        histogramExpectation.assertExpected();
        assertEquals(2, mUserActionTester.getActionCount("Tabs.RestoreArchivedTabsMenuItem"));
    }

    @Test
    @MediumTest
    public void testCloseDialogWithBackButton() {
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
    public void testRestoreAndOpenSingleTab() {
        GURL archivedUrl = new GURL("https://www.google.com");
        Tab tab = addArchivedTab(archivedUrl, "test 1");
        int tabId = tab.getId();

        addArchivedTab(new GURL("https://test.com"), "test 2");
        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(2, getTabCountOnUiThread(mArchivedTabModel));

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyTabListEditorIsHidden();
        assertEquals(2, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));

        LayoutTestUtils.waitForLayout(
                mCtaTestRule.getActivity().getLayoutManager(), LayoutType.BROWSING);
        Tab activityTab = mCtaTestRule.getActivity().getActivityTabProvider().get();
        CriteriaHelper.pollUiThread(() -> activityTab.getId() == tabId);
        assertEquals(1, mUserActionTester.getActionCount("Tabs.RestoreSingleTab"));
    }

    @Test
    @MediumTest
    public void testCloseArchivedTab() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("2 inactive items")).check(matches(isDisplayed()));

        mRobot.actionRobot.clickViewIdAtAdapterPosition(1, R.id.action_button);
        mRobot.resultRobot
                .verifyAdapterHasItemCount(1)
                .verifyUndoSnackbarWithTextIsShown("Closed google");
    }

    @Test
    @MediumTest
    public void testCloseArchivedTab_SnackbarResetForTabSwitcher() {
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("2 inactive items")).check(matches(isDisplayed()));

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
    @DisabledTest(message = "This never worked. https://crbug.com/446200399")
    public void testContentDescription() {
        onView(withContentDescription(R.string.accessibility_tab_selection_editor_back_button))
                .check(matches(isDisplayed()));
        onView(withContentDescription(R.string.accessibility_tab_selection_editor))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testBottomShadowView() {
        for (int i = 0; i < 50; i++) {
            addArchivedTab(new GURL("https://google.com?q=" + i), "test " + i);
        }

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("50 inactive items")).check(matches(isDisplayed()));
        mRobot.resultRobot.verifyTabListEditorIsVisible().verifyAdapterHasItemCount(50);

        // When there is more than a page of tabs, then the bottom container should have a shadow.
        onView(withId(R.id.close_all_tabs_button_container_shadow)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    // Scrolling all the way down on tablets is flaky.
    @Restriction(DeviceFormFactor.PHONE)
    public void testBottomShadowView_DisappersWhenFullyScrolled() {
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

        onView(withText("11 inactive items")).check(matches(isDisplayed()));
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
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    @Feature({"RenderTest"})
    public void testMessageResizedOnTablet() throws IOException {
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
    @Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
    // Flaky in automotive, https://crbug.com/462785937
    @Feature({"RenderTest"})
    public void testIphMessageResizedOnTablet() throws IOException {
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
    @Feature({"RenderTest"})
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
    public void testTabGroupMultiThumbnail_3Tabs_4Tabs_5Tabs() throws IOException {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        when(mTabGroupSyncService.getAllGroupIds())
                .thenReturn(new String[] {SYNC_GROUP_ID1, SYNC_GROUP_ID2, SYNC_GROUP_ID3});
        createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 3, true);
        createSavedTabGroup(SYNC_GROUP_ID2, GROUP_TITLE2, SYNC_GROUP_COLOR1, 4, true);
        createSavedTabGroup(SYNC_GROUP_ID3, "", SYNC_GROUP_COLOR1, 5, true);
        addArchivedTab(new GURL("https://google.com"), "test 1");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        mRenderTestRule.render(
                cta.findViewById(R.id.archived_tabs_dialog),
                "archived_tabs_saved_tab_group_multi_thumbnail");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
    public void testTabGroupMultiThumbnail_SelectableBackgroundColor() throws IOException {
        ChromeTabbedActivity cta = mCtaTestRule.getActivity();
        when(mTabGroupSyncService.getAllGroupIds())
                .thenReturn(new String[] {SYNC_GROUP_ID1, SYNC_GROUP_ID2, SYNC_GROUP_ID3});
        createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 3, true);
        createSavedTabGroup(SYNC_GROUP_ID2, GROUP_TITLE2, SYNC_GROUP_COLOR1, 4, true);
        createSavedTabGroup(SYNC_GROUP_ID3, "", SYNC_GROUP_COLOR1, 5, true);
        addArchivedTab(new GURL("https://google.com"), "test 1");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        // Verify that the thumbnail background color changes with bulk edit selection.
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select items");
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.actionRobot.clickItemAtAdapterPosition(2);

        mRenderTestRule.render(
                cta.findViewById(R.id.archived_tabs_dialog),
                "archived_tabs_saved_tab_group_multi_thumbnail_selectable_bg_color");
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
    public void testCloseAllArchivedTabs_WithSyncedTabGroups() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        SavedTabGroup savedTabGroup =
                createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 1, true);
        addArchivedTab(new GURL("https://google.com"), "test 1");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.CloseAllArchivedTabs.TabCount", 1)
                        .expectIntRecords("TabGroups.CloseAllArchivedTabGroups.TabGroupCount", 1)
                        .expectIntRecords("TabGroups.CloseAllArchivedTabGroups.TabGroupTabCount", 1)
                        .build();

        onView(withText("2 inactive items")).check(matches(isDisplayed()));
        onView(withText("Close all")).perform(click());
        onView(withText("Close all")).perform(click());

        // Assert that the group was archived and emit an event with the new archive status.
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID1, false);
        savedTabGroup.archivalTimeMs = null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> notifyTabGroupSyncObserversWithChangedGroup(savedTabGroup));

        mRobot.resultRobot.verifyTabListEditorIsHidden();
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.CloseAllArchivedTabsMenuItem"));
        assertNull(mCtaTestRule.getActivity().findViewById(R.id.archived_tabs_dialog));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
    public void testSelectCloseArchivedTabs_WithSyncedTabGroups_AndUndo() {
        SnackbarManager snackbarManager = mCtaTestRule.getActivity().getSnackbarManager();
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        SavedTabGroup savedTabGroup =
                createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 1, true);
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");
        addArchivedTab(new GURL("https://google.com"), "test 3");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(3, getTabCountOnUiThread(mArchivedTabModel));

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select items");

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.CloseArchivedTabsMenuItem.TabCount", 2)
                        .expectIntRecords("TabGroups.CloseArchivedTabsMenuItem.TabGroupCount", 1)
                        .expectIntRecords("TabGroups.CloseArchivedTabsMenuItem.TabGroupTabCount", 1)
                        .build();
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.actionRobot.clickItemAtAdapterPosition(2);
        mRobot.resultRobot.verifyToolbarSelectionText("3 items");
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Close items");

        // Assert that the group was archived and emit an event with the new archive status.
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID1, false);
        savedTabGroup.archivalTimeMs = null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> notifyTabGroupSyncObserversWithChangedGroup(savedTabGroup));

        mRobot.resultRobot
                .verifyAdapterHasItemCount(1)
                .verifyUndoSnackbarWithTextIsShown("1 tab group, 2 tabs closed");
        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.CloseArchivedTabsMenuItem"));

        // Undo the closure through the shown snackbar.
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof SavedTabGroupUndoBarController);
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        CriteriaHelper.pollUiThread(() -> 3 == getTabCountOnUiThread(mArchivedTabModel));
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID1, true);
        savedTabGroup.archivalTimeMs = System.currentTimeMillis();
        ThreadUtils.runOnUiThreadBlocking(
                () -> notifyTabGroupSyncObserversWithChangedGroup(savedTabGroup));
        onView(withText("4 inactive items")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
    public void testSelectCloseArchivedTabGroup_AndUndo() {
        SnackbarManager snackbarManager = mCtaTestRule.getActivity().getSnackbarManager();
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        SavedTabGroup savedTabGroup =
                createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 1, true);
        addArchivedTab(new GURL("https://google.com"), "test 1");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select items");

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("TabGroups.CloseArchivedTabsMenuItem.TabGroupCount", 1)
                        .expectIntRecords("TabGroups.CloseArchivedTabsMenuItem.TabGroupTabCount", 1)
                        .build();
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.resultRobot.verifyToolbarSelectionText("1 item");
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Close item");

        // Assert that the group was archived and emit an event with the new archive status.
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID1, false);
        savedTabGroup.archivalTimeMs = null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> notifyTabGroupSyncObserversWithChangedGroup(savedTabGroup));

        mRobot.resultRobot
                .verifyAdapterHasItemCount(1)
                .verifyUndoSnackbarWithTextIsShown("My Group tab group closed");
        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.CloseArchivedTabsMenuItem"));

        // Undo the closure through the shown snackbar.
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof SavedTabGroupUndoBarController);
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID1, true);
        savedTabGroup.archivalTimeMs = System.currentTimeMillis();
        ThreadUtils.runOnUiThreadBlocking(
                () -> notifyTabGroupSyncObserversWithChangedGroup(savedTabGroup));
        onView(withText("2 inactive items")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
    public void testSelectCloseArchivedTabGroups_AndUndo() {
        SnackbarManager snackbarManager = mCtaTestRule.getActivity().getSnackbarManager();
        when(mTabGroupSyncService.getAllGroupIds())
                .thenReturn(new String[] {SYNC_GROUP_ID1, SYNC_GROUP_ID2});
        SavedTabGroup savedTabGroup1 =
                createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 1, true);
        SavedTabGroup savedTabGroup2 =
                createSavedTabGroup(SYNC_GROUP_ID2, GROUP_TITLE2, SYNC_GROUP_COLOR1, 1, true);
        addArchivedTab(new GURL("https://google.com"), "test 1");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select items");

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("TabGroups.CloseArchivedTabsMenuItem.TabGroupCount", 2)
                        .expectIntRecords("TabGroups.CloseArchivedTabsMenuItem.TabGroupTabCount", 2)
                        .build();
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarSelectionText("2 items");
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Close items");

        // Assert that the group was archived and emit an event with the new archive status.
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID1, false);
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID2, false);
        savedTabGroup1.archivalTimeMs = null;
        savedTabGroup2.archivalTimeMs = null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    notifyTabGroupSyncObserversWithChangedGroup(savedTabGroup1);
                    notifyTabGroupSyncObserversWithChangedGroup(savedTabGroup2);
                });

        mRobot.resultRobot
                .verifyAdapterHasItemCount(1)
                .verifyUndoSnackbarWithTextIsShown("2 tab groups closed");
        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.CloseArchivedTabsMenuItem"));

        // Undo the closure through the shown snackbar.
        assertTrue(
                snackbarManager.getCurrentSnackbarForTesting().getController()
                        instanceof SavedTabGroupUndoBarController);
        CriteriaHelper.pollInstrumentationThread(TabUiTestHelper::verifyUndoBarShowingAndClickUndo);
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID1, true);
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID2, true);
        savedTabGroup1.archivalTimeMs = System.currentTimeMillis();
        savedTabGroup2.archivalTimeMs = System.currentTimeMillis();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    notifyTabGroupSyncObserversWithChangedGroup(savedTabGroup1);
                    notifyTabGroupSyncObserversWithChangedGroup(savedTabGroup2);
                });
        onView(withText("3 inactive items")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
    public void testSelectAllCloseArchivedTabs_WithSyncedTabGroups() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        SavedTabGroup savedTabGroup =
                createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 1, true);
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(2, getTabCountOnUiThread(mArchivedTabModel));

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select items");

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.CloseArchivedTabsMenuItem.TabCount", 2)
                        .expectIntRecords("TabGroups.CloseArchivedTabsMenuItem.TabGroupCount", 1)
                        .expectIntRecords("TabGroups.CloseArchivedTabsMenuItem.TabGroupTabCount", 1)
                        .build();
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.actionRobot.clickItemAtAdapterPosition(2);
        mRobot.resultRobot.verifyToolbarSelectionText("3 items");
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Close items");

        // Assert that the group was archived and emit an event with the new archive status.
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID1, false);
        savedTabGroup.archivalTimeMs = null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> notifyTabGroupSyncObserversWithChangedGroup(savedTabGroup));

        mRobot.resultRobot.verifyUndoSnackbarWithTextIsShown("1 tab group, 2 tabs closed");

        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.CloseArchivedTabsMenuItem"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
    public void testRestoreAllInactiveTabs_WithSyncedTabGroups() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        SavedTabGroup savedTabGroup =
                createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 1, true);
        addArchivedTab(new GURL("https://google.com"), "test 1");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        onView(withText("2 inactive items")).check(matches(isDisplayed()));

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.RestoreAllArchivedTabsMenuItem.TabCount", 1)
                        .expectIntRecords(
                                "TabGroups.RestoreAllArchivedTabsMenuItem.TabGroupCount", 1)
                        .expectIntRecords(
                                "TabGroups.RestoreAllArchivedTabsMenuItem.TabGroupTabCount", 1)
                        .build();
        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));

        // Mock the sync backend being initialized so the tab group is restored via
        // createNewTabGroup and LocalTabGroupMutationHelper, reflected in the regular tab model.
        ThreadUtils.runOnUiThreadBlocking(() -> notifyTabGroupSyncObserversWithInitialization());

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Restore all");

        // Assert that the group was unarchived and emit an event with the new archive status.
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID1, false);
        savedTabGroup.archivalTimeMs = null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> notifyTabGroupSyncObserversWithChangedGroup(savedTabGroup));

        mRobot.resultRobot.verifyTabListEditorIsHidden();
        // This count includes the restored tab group.
        assertEquals(3, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(0, getTabCountOnUiThread(mArchivedTabModel));
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.RestoreAllArchivedTabsMenuItem"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
    public void testSelectionModeMenuItem_RestoreTabGroups() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        SavedTabGroup savedTabGroup =
                createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 1, true);
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();

        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(2, getTabCountOnUiThread(mArchivedTabModel));

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select items");

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Tabs.RestoreArchivedTabsMenuItem.TabCount", 1)
                        .expectIntRecords("TabGroups.RestoreArchivedTabsMenuItem.TabGroupCount", 1)
                        .expectIntRecords(
                                "TabGroups.RestoreArchivedTabsMenuItem.TabGroupTabCount", 1)
                        .build();
        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarSelectionText("2 items");

        // Mock the sync backend being initialized so the tab group is restored via
        // createNewTabGroup and LocalTabGroupMutationHelper, reflected in the regular tab model.
        ThreadUtils.runOnUiThreadBlocking(() -> notifyTabGroupSyncObserversWithInitialization());

        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Restore items");

        // Assert that the group was unarchived and emit an event with the new archive status.
        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID1, false);
        savedTabGroup.archivalTimeMs = null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> notifyTabGroupSyncObserversWithChangedGroup(savedTabGroup));

        mRobot.resultRobot.verifyAdapterHasItemCount(1);
        // This count includes the restored tab group.
        assertEquals(3, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("Tabs.RestoreArchivedTabsMenuItem"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
    public void testCloseArchivedTabGroup_PressCloseButton() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        SavedTabGroup savedTabGroup =
                createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 1, true);
        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();
        ThreadUtils.runOnUiThreadBlocking(() -> notifyTabGroupSyncObserversWithInitialization());

        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(2, getTabCountOnUiThread(mArchivedTabModel));

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "TabGroups.ArchivedTabGroupManualCloseOnInactiveSurface.TabGroupTabCount",
                        1);

        mRobot.actionRobot.clickViewIdAtAdapterPosition(0, R.id.action_button);

        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID1, false);
        savedTabGroup.archivalTimeMs = null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> notifyTabGroupSyncObserversWithChangedGroup(savedTabGroup));
        mRobot.resultRobot.verifyAdapterHasItemCount(2);

        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(2, getTabCountOnUiThread(mArchivedTabModel));
        histogramExpectation.assertExpected();
        assertEquals(
                1,
                mUserActionTester.getActionCount(
                        "TabGroups.ArchivedTabGroupManualCloseOnInactiveSurface"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
    public void testRestoreArchivedTabGroup_ClickIntoTabGroup() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        SavedTabGroup savedTabGroupInitial =
                createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 1, true);
        SavedTabGroup savedTabGroupLocalIdSet =
                createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 1, true);
        savedTabGroupLocalIdSet.localId = new LocalTabGroupId(new Token(123L, 123L));

        addArchivedTab(new GURL("https://google.com"), "test 1");
        addArchivedTab(new GURL("https://google.com"), "test 2");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();
        ThreadUtils.runOnUiThreadBlocking(() -> notifyTabGroupSyncObserversWithInitialization());
        // Override getGroup mock defined in createSavedTabGroup to support changing state.
        when(mTabGroupSyncService.getGroup(SYNC_GROUP_ID1))
                .thenReturn(
                        savedTabGroupInitial,
                        savedTabGroupInitial,
                        savedTabGroupInitial,
                        savedTabGroupLocalIdSet,
                        savedTabGroupLocalIdSet);

        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(2, getTabCountOnUiThread(mArchivedTabModel));

        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        "TabGroups.RestoreSingleTabGroup.TabGroupTabCount", 1);

        mRobot.actionRobot.clickItemAtAdapterPosition(0);

        verify(mTabGroupSyncService).updateArchivalStatus(SYNC_GROUP_ID1, false);
        savedTabGroupLocalIdSet.archivalTimeMs = null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> notifyTabGroupSyncObserversWithChangedGroup(savedTabGroupLocalIdSet));

        assertEquals(2, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(2, getTabCountOnUiThread(mArchivedTabModel));
        histogramExpectation.assertExpected();
        assertEquals(1, mUserActionTester.getActionCount("TabGroups.RestoreSingleTabGroup"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
    public void testTabListEditorExitSelectableState_OnRemoteGroupDeleted() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        SavedTabGroup savedTabGroup =
                createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 1, true);
        addArchivedTab(new GURL("https://google.com"), "test 1");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();
        ThreadUtils.runOnUiThreadBlocking(() -> notifyTabGroupSyncObserversWithInitialization());

        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));

        // Assert that there are 2 items consisting of a tab group and 1 tab.
        onView(withText("2 inactive items")).check(matches(isDisplayed()));

        // Enter the selection state and select both the tab group and single tab.
        mRobot.actionRobot.clickToolbarMenuButton().clickToolbarMenuItem("Select items");
        mRobot.resultRobot
                .verifyAdapterHasItemCount(2)
                .verifyItemNotSelectedAtAdapterPosition(0)
                .verifyItemNotSelectedAtAdapterPosition(1)
                .verifyToolbarSelectionText("2 inactive items");

        mRobot.actionRobot.clickItemAtAdapterPosition(0);
        mRobot.actionRobot.clickItemAtAdapterPosition(1);
        mRobot.resultRobot.verifyToolbarSelectionText("2 items");

        // Mock an external event emitted which signals for the tab group deletion.
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {});
        ThreadUtils.runOnUiThreadBlocking(
                () -> notifyTabGroupSyncObserversWithDeletedGroup(savedTabGroup.syncId));

        // Verify that the selection state is now hidden.
        onView(withText("1 inactive item")).check(matches(isDisplayed()));
        mRobot.resultRobot.verifyTabListEditorIsVisible().verifyAdapterHasItemCount(1);
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.ANDROID_TAB_DECLUTTER_ARCHIVE_TAB_GROUPS})
    public void testTabListEditorTabGroupTitleUpdated_OnRemoteModification() {
        when(mTabGroupSyncService.getAllGroupIds()).thenReturn(new String[] {SYNC_GROUP_ID1});
        SavedTabGroup savedTabGroup =
                createSavedTabGroup(SYNC_GROUP_ID1, GROUP_TITLE1, SYNC_GROUP_COLOR1, 1, true);
        addArchivedTab(new GURL("https://google.com"), "test 1");

        RegularTabSwitcherStation tabSwitcherStation = mInitialPage.openRegularTabSwitcher();
        tabSwitcherStation.expectArchiveMessageCard().openArchivedTabsDialog();
        ThreadUtils.runOnUiThreadBlocking(() -> notifyTabGroupSyncObserversWithInitialization());

        assertEquals(1, getTabCountOnUiThread(mRegularTabModel));
        assertEquals(1, getTabCountOnUiThread(mArchivedTabModel));

        // Assert that there are 2 items consisting of a tab group and 1 tab.
        onView(withText("2 inactive items")).check(matches(isDisplayed()));
        onView(withText(GROUP_TITLE1)).check(matches(isDisplayed()));

        // Mock an external event emitted which updates the tab group title.
        savedTabGroup.title = GROUP_TITLE2;
        ThreadUtils.runOnUiThreadBlocking(
                () -> notifyTabGroupSyncObserversWithChangedGroup(savedTabGroup));

        // Verify that the new group title is now showing.
        onView(withText(GROUP_TITLE2)).check(matches(isDisplayed()));
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
                                        getTabCountOnUiThread(mArchivedTabModel)));
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

    private void notifyTabGroupSyncObserversWithInitialization() {
        for (TabGroupSyncService.Observer obs : mTabGroupSyncServiceObserverCaptor.getAllValues()) {
            obs.onInitialized();
        }
    }

    private void notifyTabGroupSyncObserversWithChangedGroup(SavedTabGroup group) {
        for (TabGroupSyncService.Observer obs : mTabGroupSyncServiceObserverCaptor.getAllValues()) {
            obs.onTabGroupUpdated(group, TriggerSource.REMOTE);
        }
    }

    private void notifyTabGroupSyncObserversWithDeletedGroup(String syncId) {
        for (TabGroupSyncService.Observer obs : mTabGroupSyncServiceObserverCaptor.getAllValues()) {
            obs.onTabGroupRemoved(syncId, TriggerSource.REMOTE);
        }
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
