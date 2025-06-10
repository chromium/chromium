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

import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.addBlankTabs;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.clickFirstCardFromTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.enterTabSwitcher;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.mergeAllNormalTabsToAGroup;
import static org.chromium.chrome.browser.tasks.tab_management.TabUiTestHelper.verifyTabSwitcherCardCount;
import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
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
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingUiDelegateAndroid;
import org.chromium.chrome.browser.data_sharing.FakeDataSharingUIDelegateImpl;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.data_sharing.DataSharingSDKDelegateBridge;
import org.chromium.components.data_sharing.DataSharingSDKDelegateTestImpl;
import org.chromium.components.data_sharing.DataSharingServiceImpl;
import org.chromium.components.data_sharing.GroupToken;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.test.util.DeviceRestriction;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;

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

    @Rule(order = 0)
    public SyncTestRule mActivityTestRule = new SyncTestRule();

    @Rule(order = 1)
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private FakeDataSharingUIDelegateImpl mDataSharingUIDelegate;
    private DataSharingSDKDelegateTestImpl mDataSharingSDKDelegate;

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
        DataSharingUiDelegateAndroid.setForTesting(mDataSharingUIDelegate);
        DataSharingSDKDelegateBridge.setForTesting(mDataSharingSDKDelegate);
        mActivityTestRule.getSigninTestRule().addAccount(TestAccounts.ACCOUNT1);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mProfile = mActivityTestRule.getProfile(/* incognito= */ false);
                    FirstRunStatus.setFirstRunFlowComplete(true);
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

    @Test
    @MediumTest
    public void testDataSharingUrlInterceptionRefuseSignin() {
        Assert.assertEquals(1, mActivityTestRule.tabsCount(/* incognito= */ false));
        mActivityTestRule.loadUrlInNewTab(mUrl);

        // Verify that the fullscreen sign-in promo is shown and cancel.
        onViewWaiting(withText(R.string.collaboration_signin_description))
                .check(matches(isDisplayed()));
        onView(withText(R.string.collaboration_cancel)).perform(scrollTo(), click());

        // The new data sharing url was intercepted and the tab closed.
        Assert.assertEquals(1, mActivityTestRule.tabsCount(/* incognito= */ false));
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
        Assert.assertEquals(1, mActivityTestRule.tabsCount(/* incognito= */ false));
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
                        .getString(
                                R.string.sync_promo_continue_as,
                                TestAccounts.ACCOUNT1.getGivenName());
        onView(withText(continueAsText)).perform(click());

        // Verify that the history opt-in dialog is shown and refuse.
        onViewWaiting(withText(R.string.collaboration_sync_description))
                .check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.button_secondary)).perform(click());

        // The user is signed out.
        Assert.assertNull(
                mActivityTestRule.getSigninTestRule().getPrimaryAccount(ConsentLevel.SIGNIN));
    }

    @Test
    @MediumTest
    public void testDataSharingUrlInterceptionShowsJoin() {
        mActivityTestRule.loadUrlInNewTab(mUrl);

        final AtomicBoolean joinCalled = new AtomicBoolean();
        Callback<Boolean> callback =
                (success) -> {
                    joinCalled.set(true);
                };
        mDataSharingUIDelegate.setShowJoinFlowCallback(callback);
        setFakePreviewData();

        // Verify that the fullscreen sign-in promo is shown and accept.
        onViewWaiting(withText(R.string.collaboration_signin_description))
                .check(matches(isDisplayed()));
        final String continueAsText =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string.sync_promo_continue_as,
                                TestAccounts.ACCOUNT1.getGivenName());
        onView(withText(continueAsText)).perform(click());

        // Verify that the history opt-in dialog is shown and accept.
        onViewWaiting(withText(R.string.collaboration_sync_description))
                .check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.button_primary)).perform(click());

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    return joinCalled.get();
                },
                WAIT_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testCollaborationCreateFlow() {
        final ChromeTabbedActivity cta = mActivityTestRule.getActivity();
        final AtomicBoolean createCalled = new AtomicBoolean();
        Callback<Boolean> callback =
                (success) -> {
                    createCalled.set(true);
                };
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
                        .getString(
                                R.string.sync_promo_continue_as,
                                TestAccounts.ACCOUNT1.getGivenName());
        onView(withText(continueAsText)).perform(click());

        // Verify that the history opt-in dialog is shown and accept.
        onViewWaiting(
                        withText(R.string.collaboration_sync_description),
                        // checkRootDialog=true ensures dialog is in focus, avoid flakiness.
                        true)
                .check(matches(isDisplayed()));
        onViewWaiting(withId(R.id.button_primary)).perform(click());

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    return createCalled.get();
                },
                WAIT_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    @Test
    @MediumTest
    public void testHistoryAndSyncDisabled() {
        mActivityTestRule.getSigninTestRule().addAccountThenSignin(TestAccounts.ACCOUNT1);

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

        final ChromeTabbedActivity cta = (ChromeTabbedActivity) mActivityTestRule.getActivity();
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

        // Check share button changes to manage.
        onViewWaiting(withContentDescription(R.string.manage_sharing_content_description))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testDataSharingShowShareCancelled() {
        mCollaborationTestUtils.setUpSyncAndSignIn();

        final ChromeTabbedActivity cta = (ChromeTabbedActivity) mActivityTestRule.getActivity();
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
}
