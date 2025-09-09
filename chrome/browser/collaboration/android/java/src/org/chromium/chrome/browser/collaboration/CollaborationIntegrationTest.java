// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.collaboration;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.chrome.browser.collaboration.CollaborationTestUtils.accountInfoToGroupMember;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.addBlankTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstCardFromTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.closeNthTabInDialog;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllNormalTabsToAGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;
import static org.chromium.components.signin.test.util.TestAccounts.ACCOUNT1;
import static org.chromium.components.signin.test.util.TestAccounts.ACCOUNT2;
import static org.chromium.components.signin.test.util.TestAccounts.CHILD_ACCOUNT;
import static org.chromium.components.signin.test.util.TestAccounts.MANAGED_ACCOUNT;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.graphics.Color;
import android.view.View;

import androidx.annotation.IdRes;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingUiDelegateAndroid;
import org.chromium.chrome.browser.data_sharing.FakeDataSharingUIDelegateImpl;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.CollaborationServiceShareOrManageEntryPoint;
import org.chromium.components.data_sharing.DataSharingSDKDelegateBridge;
import org.chromium.components.data_sharing.DataSharingSDKDelegateTestImpl;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.data_sharing.DataSharingServiceImpl;
import org.chromium.components.data_sharing.GroupData;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.data_sharing.member_role.MemberRole;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.tab_group_sync.EitherId.EitherGroupId;
import org.chromium.components.tab_group_sync.LocalTabGroupId;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/** Instrumentation tests for {@link CollaborationService}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.DATA_SHARING})
@DoNotBatch(reason = "Tabs can't be closed reliably between tests.")
// TODO(crbug.com/399444939) Re-enable on automotive devices if needed.
// Only run on device non-auto and with valid Google services.
@Restriction({
    DeviceRestriction.RESTRICTION_TYPE_NON_AUTO,
    GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_20W02
})
public class CollaborationIntegrationTest {

    private static final long WAIT_TIMEOUT_MS = 1000L;
    private static final String TEST_COLLABORATION_ID = "collaboration_id";
    private static final String TEST_ACCESS_TOKEN = "access_token";
    private static final String TEST_GROUP_TITLE = "group_title";

    /** Counts the number of calls from DataSharingService. Only use on UI thread. */
    private static class CountingShareObserver implements DataSharingService.Observer {
        public int groupChangedCount;
        public int groupAddedCount;

        @Override
        public void onGroupChanged(GroupData groupData) {
            groupChangedCount++;
        }

        @Override
        public void onGroupAdded(GroupData groupData) {
            groupAddedCount++;
        }
    }

    private static class CountingSyncObserver implements TabGroupSyncService.Observer {
        public int nonNullTabGroupLocalIdChangedCount;

        @Override
        public void onTabGroupLocalIdChanged(
                String syncTabGroupId, @Nullable LocalTabGroupId localTabGroupId) {
            // Ignore calls while localTabGroupId is null, this hasn't been linked yet. This roughly
            // helps up match conditions the SharedGroupObserver is looking for.
            if (localTabGroupId != null) {
                nonNullTabGroupLocalIdChangedCount++;
            }
        }
    }

    @Rule(order = 0)
    public SyncTestRule mActivityTestRule = new SyncTestRule();

    @Rule(order = 1)
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE_TAB_GROUPS)
                    .setRevision(1)
                    .build();

    private FakeDataSharingUIDelegateImpl mDataSharingUIDelegate;
    private DataSharingSDKDelegateTestImpl mDataSharingSDKDelegate;
    private CountingShareObserver mCountingShareObserver;
    private CountingSyncObserver mCountingSyncObserver;

    private Profile mProfile;
    private String mUrl;
    private CollaborationTestUtils mCollaborationTestUtils;

    public CollaborationIntegrationTest() {
        DataSharingUiDelegateAndroid.setForTesting(mDataSharingUIDelegate);
        DataSharingSDKDelegateBridge.setForTesting(mDataSharingSDKDelegate);
    }

    @Before
    public void setUp() {
        mDataSharingUIDelegate = new FakeDataSharingUIDelegateImpl();
        mDataSharingSDKDelegate = new DataSharingSDKDelegateTestImpl();
        mCountingShareObserver = new CountingShareObserver();
        mCountingSyncObserver = new CountingSyncObserver();
        DataSharingUiDelegateAndroid.setForTesting(mDataSharingUIDelegate);
        DataSharingSDKDelegateBridge.setForTesting(mDataSharingSDKDelegate);
        mActivityTestRule.getSigninTestRule().addAccount(ACCOUNT1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile = mActivityTestRule.getProfile(/* incognito= */ false);
                    FirstRunStatus.setFirstRunFlowComplete(true);
                    DataSharingServiceFactory.getForProfile(mProfile)
                            .addObserver(mCountingShareObserver);
                    TabGroupSyncServiceFactory.getForProfile(mProfile)
                            .addObserver(mCountingSyncObserver);
                });
        mUrl =
                DataSharingServiceImpl.getDataSharingUrlForTesting(
                                new GroupToken(TEST_COLLABORATION_ID, "access_token"))
                        .getSpec();

        mCollaborationTestUtils = new CollaborationTestUtils(mActivityTestRule, mProfile);
    }

    /* Sets up preview data for the group ID. */
    private void setFakePreviewData() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DataSharingServiceImpl service =
                            (DataSharingServiceImpl)
                                    DataSharingServiceFactory.getForProfile(
                                            mActivityTestRule.getProfile(false));
                    service.setSharedEntitiesPreviewForTesting(TEST_COLLABORATION_ID);
                });
    }

    private LocalTabGroupId createTabGroup() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        addBlankTabs(cta, false, 2);
        mergeAllNormalTabsToAGroup(cta);
        return mCollaborationTestUtils.getLocalTabGroupId(cta);
    }

    private void linkLocalGroupToSharedIdForOwner(
            LocalTabGroupId localTabGroupId, String collaborationId, AccountInfo accountInfo) {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        Callback<Boolean> callback =
                (success) -> {
                    mDataSharingSDKDelegate.addMembers(
                            collaborationId,
                            accountInfoToGroupMember(accountInfo, MemberRole.OWNER));
                    mActivityTestRule.getFakeServerHelper().addCollaboration(collaborationId);
                    mDataSharingUIDelegate.forceGroupCreation(collaborationId, TEST_ACCESS_TOKEN);
                };
        mDataSharingUIDelegate.setShowCreateFlowCallback(callback);
        mCollaborationTestUtils.setupShareDelegateSupplier(cta);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    cta.getRootUiCoordinatorForTesting()
                            .getDataSharingTabManager()
                            .createOrManageFlow(
                                    EitherGroupId.createLocalId(localTabGroupId),
                                    CollaborationServiceShareOrManageEntryPoint.UNKNOWN,
                                    (ignored) -> {});
                });

        // There's many async signals that fly around after linking the tab group. In particular we
        // need the SharedGroupObserver to think the group is shared. For this, it already knows the
        // tab group id it should watch for, but it needs to see that it's now associated with a
        // valid collaboration id. And while the DataSharingService is what has member information
        // for a collaboration id, the SharedGroupObserver cannot link its tab group id to the
        // collaboration id that it'll be informed about until teh TabGroupSyncService lets it know
        // about the mapping. Once both of these happen, we're safe to continue, but the order of
        // these is not deterministic and we must wait for both.
        int startingAddedCount =
                ThreadUtils.runOnUiThreadBlocking(() -> mCountingShareObserver.groupChangedCount);
        int startingNonNullSyncCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mCountingSyncObserver.nonNullTabGroupLocalIdChangedCount);
        mCollaborationTestUtils.prepareToShareGroup(localTabGroupId, collaborationId);
        CriteriaHelper.pollUiThread(
                () -> mCountingShareObserver.groupAddedCount > startingAddedCount);
        CriteriaHelper.pollUiThread(
                () ->
                        mCountingSyncObserver.nonNullTabGroupLocalIdChangedCount
                                > startingNonNullSyncCount);
    }

    private void memberJoinsSharedGroup(String collaborationId, AccountInfo accountInfo) {
        mDataSharingSDKDelegate.addMembers(
                collaborationId, accountInfoToGroupMember(accountInfo, MemberRole.MEMBER));
        // Will override the previous specifics with a different consistency_token.
        mActivityTestRule.getFakeServerHelper().addCollaborationGroupToFakeServer(collaborationId);

        int startingChangedCount =
                ThreadUtils.runOnUiThreadBlocking(() -> mCountingShareObserver.groupChangedCount);
        SyncTestUtil.triggerSync();
        CriteriaHelper.pollUiThread(
                () -> mCountingShareObserver.groupChangedCount > startingChangedCount);
    }

    @Test
    @MediumTest
    public void testDataSharingUrlInterceptionRefuseSignin() {
        assertEquals(1, mActivityTestRule.tabsCount(/* incognito= */ false));
        mActivityTestRule.loadUrlInNewTab(mUrl);

        // Verify that the fullscreen sign-in promo is shown and cancel.
        onViewWaiting(withText(R.string.collaboration_signin_description))
                .check(matches(isDisplayed()));
        onView(withText(R.string.collaboration_cancel)).perform(scrollTo(), click());

        // The new data sharing url was intercepted and the tab closed.
        assertEquals(1, mActivityTestRule.tabsCount(/* incognito= */ false));
    }

    @Test
    @MediumTest
    public void testDataSharingUrlInterceptionExternalAppRefuseSignin() {
        mActivityTestRule.loadUrlInNewTab(
                mUrl, /* incognito= */ false, TabLaunchType.FROM_EXTERNAL_APP);

        // Verify that the fullscreen sign-in promo is shown and cancel.
        onViewWaiting(withText(R.string.collaboration_signin_description))
                .check(matches(isDisplayed()));
        onView(withText(R.string.collaboration_cancel)).perform(scrollTo(), click());

        // The new data sharing url was intercepted and the tab closed.
        assertEquals(1, mActivityTestRule.tabsCount(/* incognito= */ false));
    }

    @Test
    @MediumTest
    public void testDataSharingUrlInterceptionRefuseSync() {
        mActivityTestRule.loadUrlInNewTab(mUrl);

        // Verify that the fullscreen sign-in promo is shown and accept.
        onViewWaiting(withText(R.string.collaboration_signin_description))
                .check(matches(isDisplayed()));
        final String continueAsText =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.sync_promo_continue_as, ACCOUNT1.getGivenName());
        onView(withText(continueAsText)).perform(click());

        // Verify that the history opt-in dialog is shown and refuse.
        onViewWaiting(withText(R.string.collaboration_sync_description))
                .check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.button_secondary)).perform(click());

        // The user is signed out.
        assertNull(mActivityTestRule.getSigninTestRule().getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    public void testDataSharingUrlInterceptionShowsJoin() {
        mActivityTestRule.loadUrlInNewTab(mUrl);

        final AtomicBoolean joinCalled = new AtomicBoolean();
        Callback<Boolean> callback = (success) -> joinCalled.set(true);
        mDataSharingUIDelegate.setShowJoinFlowCallback(callback);
        setFakePreviewData();

        // Verify that the fullscreen sign-in promo is shown and accept.
        onViewWaiting(withText(R.string.collaboration_signin_description))
                .check(matches(isDisplayed()));
        final String continueAsText =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.sync_promo_continue_as, ACCOUNT1.getGivenName());
        onView(withText(continueAsText)).perform(click());

        // Verify that the history opt-in dialog is shown and accept.
        onViewWaiting(withText(R.string.collaboration_sync_description))
                .check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.button_primary)).perform(click());

        CriteriaHelper.pollInstrumentationThread(
                joinCalled::get, WAIT_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testCollaborationCreateFlow() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        final AtomicBoolean createCalled = new AtomicBoolean();
        Callback<Boolean> callback = (success) -> createCalled.set(true);
        mDataSharingUIDelegate.setShowCreateFlowCallback(callback);

        // Create a tab group and enter TabGridDialog.
        addBlankTabs(cta, false, 3);
        enterTabSwitcher(cta);
        mergeAllNormalTabsToAGroup(cta);
        verifyTabSwitcherCardCount(cta, 1);
        clickFirstCardFromTabSwitcher(cta);
        onViewWaiting(withId(R.id.share_button)).check(matches(isDisplayed())).perform(click());

        // Verify that bottom sheet sign-in is shown and accept.
        onViewWaiting(
                allOf(
                        withText(R.string.collaboration_signin_bottom_sheet_description),
                        isDisplayed()));
        final String continueAsText =
                mActivityTestRule
                        .getActivity()
                        .getString(R.string.sync_promo_continue_as, ACCOUNT1.getGivenName());
        onView(withText(continueAsText)).perform(click());

        // Verify that the history opt-in dialog is shown and accept.
        onViewWaiting(
                        withText(R.string.collaboration_sync_description),
                        // checkRootDialog=true ensures dialog is in focus, avoid flakiness.
                        true)
                .check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.button_primary)).perform(click());

        CriteriaHelper.pollInstrumentationThread(
                createCalled::get, WAIT_TIMEOUT_MS, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testHistoryAndSyncDisabled() {
        mActivityTestRule.getSigninTestRule().addAccountThenSignin(ACCOUNT1);

        mActivityTestRule.loadUrlInNewTab(
                mUrl, /* incognito= */ false, TabLaunchType.FROM_EXTERNAL_APP);
        // Verify that the history opt-in dialog is shown and refuse.
        onViewWaiting(withText(R.string.collaboration_sync_description))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testDataSharingShowShare() {
        mCollaborationTestUtils.setUpSyncAndSignIn();

        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mCollaborationTestUtils.createTabGroupAndOpenTabGridDialog(cta);

        // Setting create flow callback to show share sheet with share link.
        Callback<Boolean> callback =
                (success) -> {
                    mActivityTestRule.getFakeServerHelper().addCollaboration(TEST_COLLABORATION_ID);
                    mDataSharingUIDelegate.forceGroupCreation(
                            TEST_COLLABORATION_ID, TEST_ACCESS_TOKEN);
                };
        mDataSharingUIDelegate.setShowCreateFlowCallback(callback);
        mCollaborationTestUtils.setupShareDelegateSupplier(cta);

        onView(withId(R.id.share_button)).perform(CollaborationTestUtils.relaxedClick());
        mCollaborationTestUtils.prepareToShareGroup(
                mCollaborationTestUtils.getLocalTabGroupId(cta), TEST_COLLABORATION_ID);

        // Assert that sdk delegate has been initialized.
        assertTrue(
                "DataSharingSDKDelegateBridge should be initialized for shared tab group.",
                DataSharingSDKDelegateBridge.isInitializedForTesting());

        // Check share button changes to manage.
        onViewWaiting(withContentDescription(R.string.manage_sharing_content_description))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testDataSharingShowShareCancelled() {
        mCollaborationTestUtils.setUpSyncAndSignIn();

        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mCollaborationTestUtils.createTabGroupAndOpenTabGridDialog(cta);

        // Setting create flow callback to cancel create flow.
        Callback<Boolean> callback =
                (success) -> {
                    mDataSharingUIDelegate.forceCreateFlowCancellation();
                };
        mDataSharingUIDelegate.setShowCreateFlowCallback(callback);

        onView(withId(R.id.share_button)).perform(CollaborationTestUtils.relaxedClick());

        // Check share button doesn't change.
        onViewWaiting(withText(R.string.tab_grid_share_button_text)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Flaky test, see crbug.com/441333492")
    public void testDataSharingDeleteGroup() {
        mCollaborationTestUtils.setUpSyncAndSignIn();
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        mCollaborationTestUtils.createTabGroupAndOpenTabGridDialog(cta);

        // Setting create flow callback to show share sheet with share link.
        Callback<Boolean> callback =
                (success) -> {
                    mDataSharingSDKDelegate.addMembers(
                            TEST_COLLABORATION_ID,
                            accountInfoToGroupMember(ACCOUNT1, MemberRole.OWNER));
                    mActivityTestRule.getFakeServerHelper().addCollaboration(TEST_COLLABORATION_ID);
                    mDataSharingUIDelegate.forceGroupCreation(
                            TEST_COLLABORATION_ID, TEST_ACCESS_TOKEN);
                };
        mDataSharingUIDelegate.setShowCreateFlowCallback(callback);
        mCollaborationTestUtils.setupShareDelegateSupplier(cta);

        onView(withId(R.id.share_button)).perform(CollaborationTestUtils.relaxedClick());
        mCollaborationTestUtils.prepareToShareGroup(
                mCollaborationTestUtils.getLocalTabGroupId(cta), TEST_COLLABORATION_ID);

        // Check share button changes to manage.
        onViewWaiting(withContentDescription(R.string.manage_sharing_content_description))
                .check(matches(isDisplayed()));

        // Click "Delete group" from the menu.
        onViewWaiting(withId(R.id.toolbar_menu_button))
                .perform(CollaborationTestUtils.relaxedClick());
        onViewWaiting(withText(R.string.tab_grid_dialog_toolbar_delete_group)).perform(click());

        // Click "Delete group" in confirmation dialog.
        onViewWaiting(withText(R.string.delete_tab_group_action)).perform(click());

        // Verify that the group is deleted and the card is gone from the tab switcher.
        CriteriaHelper.pollUiThread(
                () -> cta.getTabModelSelector().getTotalTabCount() == 0, "Tabs not closed.");
        CriteriaHelper.pollInstrumentationThread(
                () ->
                        !mCollaborationTestUtils.collaborationExistsInSyncService(
                                TEST_COLLABORATION_ID),
                "Collaboration still exists.");
        verifyTabSwitcherCardCount(cta, 0);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "Flaky, see crbug.com/440302092")
    public void testDataSharingLeaveGroup() {
        mCollaborationTestUtils.setUpSyncAndSignIn();
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        mCollaborationTestUtils.createTabGroupAndOpenTabGridDialog(cta);

        // Setting create flow callback to show share sheet with share link.
        Callback<Boolean> callback =
                (success) -> {
                    mDataSharingSDKDelegate.addMembers(
                            TEST_COLLABORATION_ID,
                            accountInfoToGroupMember(ACCOUNT1, MemberRole.MEMBER));
                    mActivityTestRule.getFakeServerHelper().addCollaboration(TEST_COLLABORATION_ID);
                    mDataSharingUIDelegate.forceGroupCreation(
                            TEST_COLLABORATION_ID, TEST_ACCESS_TOKEN);
                };
        mDataSharingUIDelegate.setShowCreateFlowCallback(callback);
        mCollaborationTestUtils.setupShareDelegateSupplier(cta);

        onView(withId(R.id.share_button)).perform(CollaborationTestUtils.relaxedClick());
        mCollaborationTestUtils.prepareToShareGroup(
                mCollaborationTestUtils.getLocalTabGroupId(cta), TEST_COLLABORATION_ID);

        // Check share button changes to manage.
        onViewWaiting(withContentDescription(R.string.manage_sharing_content_description))
                .check(matches(isDisplayed()));

        // Click "Leave group" from the menu.
        onViewWaiting(withId(R.id.toolbar_menu_button))
                .perform(CollaborationTestUtils.relaxedClick());
        onViewWaiting(withText(R.string.tab_grid_dialog_toolbar_leave_group)).perform(click());

        // Click "Leave group" in confirmation dialog.
        onViewWaiting(withText(R.string.keep_tab_group_dialog_leave_action)).perform(click());

        // Verify that the group is deleted and the card is gone from the tab switcher.
        CriteriaHelper.pollUiThread(
                () -> cta.getTabModelSelector().getTotalTabCount() == 0, "Tabs not closed.");
        CriteriaHelper.pollInstrumentationThread(
                () ->
                        !mCollaborationTestUtils.collaborationExistsInSyncService(
                                TEST_COLLABORATION_ID),
                "Collaboration still exists.");
        verifyTabSwitcherCardCount(cta, 0);
    }

    @Test
    @MediumTest
    public void testDataSharingCloseGroup() {
        mCollaborationTestUtils.setUpSyncAndSignIn();
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        mCollaborationTestUtils.createTabGroupAndOpenTabGridDialog(cta);

        // Setting create flow callback to show share sheet with share link.
        Callback<Boolean> callback =
                (success) -> {
                    mDataSharingSDKDelegate.addMembers(
                            TEST_COLLABORATION_ID,
                            accountInfoToGroupMember(ACCOUNT1, MemberRole.MEMBER));
                    mActivityTestRule.getFakeServerHelper().addCollaboration(TEST_COLLABORATION_ID);
                    mDataSharingUIDelegate.forceGroupCreation(
                            TEST_COLLABORATION_ID, TEST_ACCESS_TOKEN);
                };
        mDataSharingUIDelegate.setShowCreateFlowCallback(callback);
        mCollaborationTestUtils.setupShareDelegateSupplier(cta);

        onView(withId(R.id.share_button)).perform(CollaborationTestUtils.relaxedClick());
        mCollaborationTestUtils.prepareToShareGroup(
                mCollaborationTestUtils.getLocalTabGroupId(cta), TEST_COLLABORATION_ID);

        // Check share button changes to manage.
        onViewWaiting(withContentDescription(R.string.manage_sharing_content_description))
                .check(matches(isDisplayed()));

        // Click "Close group" from the menu.
        onViewWaiting(withId(R.id.toolbar_menu_button))
                .perform(CollaborationTestUtils.relaxedClick());
        onViewWaiting(withText(R.string.tab_grid_dialog_toolbar_close_group)).perform(click());

        // Dismiss the undo snackbar.
        ThreadUtils.runOnUiThreadBlocking(() -> cta.getTabModelSelector().commitAllTabClosures());

        // Verify that the group is deleted and the card is gone from the tab switcher.
        CriteriaHelper.pollUiThread(
                () -> cta.getTabModelSelector().getTotalTabCount() == 0, "Tabs not closed.");
        verifyTabSwitcherCardCount(cta, 0);

        // Verify that the collaboration exists.
        assertTrue(mCollaborationTestUtils.collaborationExistsInSyncService(TEST_COLLABORATION_ID));
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/429200485 - window focus is lost for the keep action.")
    public void testDataSharingKeepLastTabInGroup() {
        mCollaborationTestUtils.setUpSyncAndSignIn();
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();

        mCollaborationTestUtils.createTabGroupAndOpenTabGridDialog(cta);

        // Setting create flow callback to show share sheet with share link.
        Callback<Boolean> callback =
                (success) -> {
                    mDataSharingSDKDelegate.addMembers(
                            TEST_COLLABORATION_ID,
                            accountInfoToGroupMember(ACCOUNT1, MemberRole.OWNER));
                    mActivityTestRule.getFakeServerHelper().addCollaboration(TEST_COLLABORATION_ID);
                    mDataSharingUIDelegate.forceGroupCreation(
                            TEST_COLLABORATION_ID, TEST_ACCESS_TOKEN);
                };
        mDataSharingUIDelegate.setShowCreateFlowCallback(callback);
        mCollaborationTestUtils.setupShareDelegateSupplier(cta);

        onView(withId(R.id.share_button)).perform(CollaborationTestUtils.relaxedClick());
        mCollaborationTestUtils.prepareToShareGroup(
                mCollaborationTestUtils.getLocalTabGroupId(cta), TEST_COLLABORATION_ID);

        // Check share button changes to manage.
        onViewWaiting(withContentDescription(R.string.manage_sharing_content_description))
                .check(matches(isDisplayed()));

        // Close each tab.
        List<Integer> tabIds =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            TabModel tabModel = cta.getTabModelSelector().getModel(false);
                            List<Integer> ids = new ArrayList<>();
                            for (Tab tab : tabModel) {
                                ids.add(tab.getId());
                            }
                            return ids;
                        });
        for (int i = 0; i < tabIds.size(); i++) {
            closeNthTabInDialog(0);
        }

        // Click keep group.
        onViewWaiting(withText(R.string.keep_tab_group_dialog_keep_action)).perform(click());

        // Verify that the group is kept, but a replacement tab is created.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModel tabModel = cta.getTabModelSelector().getModel(false);
                    assertEquals(1, tabModel.getCount());
                    assertFalse(tabIds.contains(tabModel.getTabAt(0).getId()));
                });

        // Verify that the collaboration exists.
        assertTrue(mCollaborationTestUtils.collaborationExistsInSyncService(TEST_COLLABORATION_ID));
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTilesDialogRender() throws Exception {
        mDataSharingUIDelegate.overrideAvatarColor(ACCOUNT1.getGaiaId(), Color.RED);
        mDataSharingUIDelegate.overrideAvatarColor(ACCOUNT2.getGaiaId(), Color.BLUE);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mCollaborationTestUtils.setUpSyncAndSignIn();

        LocalTabGroupId localTabGroupId = createTabGroup();
        enterTabSwitcher(cta);
        clickFirstCardFromTabSwitcher(cta);
        CriteriaHelper.pollUiThread(() -> CollaborationTestUtils.isDialogFullyVisible(cta));

        @IdRes int targetId = R.id.tab_group_toolbar;
        mRenderTestRule.render(cta.findViewById(targetId), "tiles_dialog_not_shared");

        linkLocalGroupToSharedIdForOwner(localTabGroupId, TEST_COLLABORATION_ID, ACCOUNT1);
        mRenderTestRule.render(cta.findViewById(targetId), "tiles_dialog_only_owner");

        memberJoinsSharedGroup(TEST_COLLABORATION_ID, ACCOUNT2);
        mRenderTestRule.render(cta.findViewById(targetId), "tiles_dialog_two_faces");

        memberJoinsSharedGroup(TEST_COLLABORATION_ID, CHILD_ACCOUNT);
        mRenderTestRule.render(cta.findViewById(targetId), "tiles_dialog_three_faces");

        memberJoinsSharedGroup(TEST_COLLABORATION_ID, MANAGED_ACCOUNT);
        mRenderTestRule.render(cta.findViewById(targetId), "tiles_dialog_two_plus_count");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testTilesGroupPaneRender() throws Exception {
        mDataSharingUIDelegate.overrideAvatarColor(ACCOUNT1.getGaiaId(), Color.RED);
        mDataSharingUIDelegate.overrideAvatarColor(ACCOUNT2.getGaiaId(), Color.BLUE);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mCollaborationTestUtils.setUpSyncAndSignIn();

        LocalTabGroupId localTabGroupId = createTabGroup();
        enterTabSwitcher(cta);
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        cta.getHubManagerSupplierForTesting()
                                .get()
                                .getPaneManager()
                                .focusPane(PaneId.TAB_GROUPS));

        RecyclerView recyclerView = cta.findViewById(R.id.tab_group_list_recycler_view);
        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);
        RecyclerView.LayoutManager layoutManager = recyclerView.getLayoutManager();
        mRenderTestRule.render(layoutManager.getChildAt(0), "tiles_group_pane_not_shared");

        linkLocalGroupToSharedIdForOwner(localTabGroupId, TEST_COLLABORATION_ID, ACCOUNT1);
        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);
        mRenderTestRule.render(layoutManager.getChildAt(0), "tiles_group_pane_only_owner");

        memberJoinsSharedGroup(TEST_COLLABORATION_ID, ACCOUNT2);
        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);
        mRenderTestRule.render(layoutManager.getChildAt(0), "tiles_group_pane_two_faces");

        memberJoinsSharedGroup(TEST_COLLABORATION_ID, CHILD_ACCOUNT);
        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);
        mRenderTestRule.render(layoutManager.getChildAt(0), "tiles_group_pane_three_faces");

        memberJoinsSharedGroup(TEST_COLLABORATION_ID, MANAGED_ACCOUNT);
        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);
        mRenderTestRule.render(layoutManager.getChildAt(0), "tiles_group_pane_two_plus_count");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testTilesBottomStripRender() throws Exception {
        mDataSharingUIDelegate.overrideAvatarColor(ACCOUNT1.getGaiaId(), Color.RED);
        mDataSharingUIDelegate.overrideAvatarColor(ACCOUNT2.getGaiaId(), Color.BLUE);
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        mCollaborationTestUtils.setUpSyncAndSignIn();

        LocalTabGroupId localTabGroupId = createTabGroup();
        CriteriaHelper.pollUiThread(() -> cta.findViewById(R.id.tab_group_ui_toolbar_view) != null);
        View targetView = cta.findViewById(R.id.tab_group_ui_toolbar_view);
        // While the tiles are not within the RecyclerView, the tab favicons do have a RecyclerView.
        CriteriaHelper.pollUiThread(
                () -> targetView.findViewById(R.id.tab_list_recycler_view) != null);
        RecyclerView recyclerView = targetView.findViewById(R.id.tab_list_recycler_view);
        RecyclerViewTestUtils.waitForStableRecyclerView(recyclerView);
        mRenderTestRule.render(targetView, "tiles_bottom_strip_not_shared");

        linkLocalGroupToSharedIdForOwner(localTabGroupId, TEST_COLLABORATION_ID, ACCOUNT1);
        mRenderTestRule.render(targetView, "tiles_bottom_strip_only_owner");

        memberJoinsSharedGroup(TEST_COLLABORATION_ID, ACCOUNT2);
        mRenderTestRule.render(targetView, "tiles_bottom_strip_two_faces");

        memberJoinsSharedGroup(TEST_COLLABORATION_ID, CHILD_ACCOUNT);
        mRenderTestRule.render(targetView, "tiles_bottom_strip_three_faces");

        memberJoinsSharedGroup(TEST_COLLABORATION_ID, MANAGED_ACCOUNT);
        mRenderTestRule.render(targetView, "tiles_bottom_strip_two_plus_count");
    }
}
