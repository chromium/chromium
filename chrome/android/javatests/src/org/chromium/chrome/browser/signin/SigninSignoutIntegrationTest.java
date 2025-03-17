// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressBack;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.contrib.RecyclerViewActions.scrollTo;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isRoot;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.core.AllOf.allOf;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.MainSettings;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtilsJni;
import org.chromium.chrome.browser.sync.settings.AccountManagementFragment;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncHelper;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.BookmarkTestUtil;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.test.util.MockitoHelper;
import org.chromium.url.GURL;

/** Test the lifecycle of sign-in and sign-out. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class SigninSignoutIntegrationTest {
    @Rule
    public final SettingsActivityTestRule<AccountManagementFragment> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AccountManagementFragment.class);

    private final SettingsActivityTestRule<MainSettings> mMainSettingsActivityTestRule =
            new SettingsActivityTestRule<>(MainSettings.class);

    private final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private final SigninTestRule mSigninTestRule = new SigninTestRule();

    // Mock sign-in environment needs to be destroyed after ChromeActivity in case there are
    // observers registered in the AccountManagerFacade mock.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule)
                    .around(mActivityTestRule)
                    .around(mMainSettingsActivityTestRule);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private ExternalAuthUtils mExternalAuthUtilsMock;

    @Mock private SigninMetricsUtils.Natives mSigninMetricsUtilsNativeMock;

    @Mock private SigninManager.SignInStateObserver mSignInStateObserverMock;

    @Mock private HistorySyncHelper mHistorySyncHelper;

    private SigninManager mSigninManager;

    private BookmarkModel mBookmarkModel;

    @Before
    public void setUp() {
        SigninMetricsUtilsJni.setInstanceForTesting(mSigninMetricsUtilsNativeMock);
        mActivityTestRule.startMainActivityOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSigninManager =
                            IdentityServicesProvider.get()
                                    .getSigninManager(mActivityTestRule.getProfile(false));
                    mSigninManager.addSignInStateObserver(mSignInStateObserverMock);
                });
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> mSigninManager.removeSignInStateObserver(mSignInStateObserverMock));
    }

    @Test
    @LargeTest
    public void testSignIn() {
        var signinHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SignIn.Completed", SigninAccessPoint.SETTINGS);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
        HistorySyncHelper.setInstanceForTesting(mHistorySyncHelper);
        doReturn(true).when(mHistorySyncHelper).shouldSuppressHistorySync();
        mSigninTestRule.addAccount(TestAccounts.ACCOUNT1);
        mMainSettingsActivityTestRule.startSettingsActivity();

        onView(withId(R.id.recycler_view))
                .perform(scrollTo(hasDescendant(withText(R.string.signin_settings_title))));
        onView(withText(R.string.signin_settings_title)).perform(click());
        onView(
                        allOf(
                                withId(R.id.account_picker_continue_as_button),
                                withParent(withId(R.id.account_picker_state_collapsed))))
                .perform(click());

        CriteriaHelper.pollUiThread(
                () -> mSigninManager.getIdentityManager().hasPrimaryAccount(ConsentLevel.SIGNIN));
        verify(mSignInStateObserverMock).onSignedIn();
        verify(mSignInStateObserverMock, never()).onSignedOut();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            TestAccounts.ACCOUNT1,
                            mSigninManager
                                    .getIdentityManager()
                                    .getPrimaryAccountInfo(ConsentLevel.SIGNIN));
                    Assert.assertTrue(
                            mSigninManager.getIdentityManager().isClearPrimaryAccountAllowed());
                });
        signinHistogram.assertExpected(
                "Signin should be recorded with the settings page as the access point.");
    }

    @Test
    @LargeTest
    public void testSignInNonDisplayableAccountEmail() {
        mSigninTestRule.addAccount(TestAccounts.CHILD_ACCOUNT_NON_DISPLAYABLE_EMAIL);

        // The child account will be automatically signed in.
        CriteriaHelper.pollUiThread(
                () -> mSigninManager.getIdentityManager().hasPrimaryAccount(ConsentLevel.SIGNIN));
        verify(mSignInStateObserverMock).onSignedIn();
    }

    @Test
    @LargeTest
    public void testSignOut() {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        mSettingsActivityTestRule.startSettingsActivity();
        onView(withText(R.string.sign_out_and_turn_off_sync)).perform(click());
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        assertSignedOut();
        MockitoHelper.waitForEvent(mSignInStateObserverMock).onSignedOut();
    }

    @Test
    @LargeTest
    public void testSignOutDismissedByPressingBack() {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        mSettingsActivityTestRule.startSettingsActivity();
        onView(withText(R.string.sign_out_and_turn_off_sync)).perform(click());
        onView(isRoot()).perform(pressBack());
        verify(mSignInStateObserverMock, never()).onSignedOut();
        assertSignedIn();
    }

    @Test
    @LargeTest
    public void testSignOutCancelled() {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        mSettingsActivityTestRule.startSettingsActivity();
        onView(withText(R.string.sign_out_and_turn_off_sync)).perform(click());
        onView(withText(R.string.cancel)).inRoot(isDialog()).perform(click());
        verify(mSignInStateObserverMock, never()).onSignedOut();
        assertSignedIn();
    }

    @Test
    @LargeTest
    public void testSignOutNonManagedAccountWithDataWiped() {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        addOneTestBookmark();
        mSettingsActivityTestRule.startSettingsActivity();
        onView(withText(R.string.sign_out_and_turn_off_sync)).perform(click());
        onView(withId(R.id.remove_local_data)).perform(click());
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        assertSignedOut();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            0,
                            mBookmarkModel.getChildCount(
                                    mBookmarkModel.getDefaultBookmarkFolder()));
                });
    }

    @Test
    @LargeTest
    public void testSignOutNonManagedAccountWithoutWipingData() {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        addOneTestBookmark();
        mSettingsActivityTestRule.startSettingsActivity();
        onView(withText(R.string.sign_out_and_turn_off_sync)).perform(click());
        onView(withText(R.string.continue_button)).inRoot(isDialog()).perform(click());
        assertSignedOut();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            1,
                            mBookmarkModel.getChildCount(
                                    mBookmarkModel.getDefaultBookmarkFolder()));
                });
    }

    @Test
    @LargeTest
    public void testChildAccountSignIn() {
        mSigninTestRule.addChildTestAccountThenWaitForSignin();
        CriteriaHelper.pollUiThread(
                () -> mSigninManager.getIdentityManager().hasPrimaryAccount(ConsentLevel.SIGNIN));

        verify(mSignInStateObserverMock).onSignedIn();
        verify(mSignInStateObserverMock, never()).onSignedOut();
        onView(withText(R.string.account_management_sign_out)).check(doesNotExist());
    }

    private void addOneTestBookmark() {
        Assert.assertNull("This method should be called only once!", mBookmarkModel);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mBookmarkModel =
                            BookmarkModel.getForProfile(
                                    mActivityTestRule.getActivity().getActivityTab().getProfile());
                    mBookmarkModel.loadFakePartnerBookmarkShimForTesting();
                });
        BookmarkTestUtil.waitForBookmarkModelLoaded();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            0,
                            mBookmarkModel.getChildCount(
                                    mBookmarkModel.getDefaultBookmarkFolder()));
                    mBookmarkModel.addBookmark(
                            mBookmarkModel.getDefaultBookmarkFolder(),
                            0,
                            "Test Bookmark",
                            new GURL("http://google.com"));
                    Assert.assertEquals(
                            1,
                            mBookmarkModel.getChildCount(
                                    mBookmarkModel.getDefaultBookmarkFolder()));
                });
    }

    private void assertSignedIn() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            "Account should be signed in!",
                            mSigninManager
                                    .getIdentityManager()
                                    .hasPrimaryAccount(ConsentLevel.SYNC));
                });
    }

    private void assertSignedOut() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(
                            "Account should be signed out!",
                            mSigninManager
                                    .getIdentityManager()
                                    .hasPrimaryAccount(ConsentLevel.SIGNIN));
                });
    }
}
