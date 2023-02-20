// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.signin;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.accounts.Account;
import android.app.Activity;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.lifecycle.Stage;

import androidx.fragment.app.Fragment;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.metrics.HistogramTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CommandLineFlags.Add;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunPageDelegate;
import org.chromium.chrome.browser.firstrun.SyncConsentFirstRunFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils.State;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;
import java.util.List;
import java.util.Set;

/**
 * Render tests for sync consent fragment.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.FORCE_ENABLE_SIGNIN_FRE})
public class SyncConsentFragmentTest {
    private static final int RENDER_REVISION = 1;
    private static final String RENDER_DESCRIPTION = "Change button style";
    private static final String NEW_ACCOUNT_NAME = "new.account@gmail.com";
    // TODO(https://crbug.com/1414078): Use ALL_SELECTABLE_TYPES defined in {@link SyncServiceImpl}
    // here.
    private static final Set<Integer> ALL_CLANK_SYNCABLE_DATA_TYPES = Set.of(
            UserSelectableType.AUTOFILL, UserSelectableType.BOOKMARKS, UserSelectableType.PASSWORDS,
            UserSelectableType.PREFERENCES, UserSelectableType.TABS, UserSelectableType.HISTORY,
            UserSelectableType.READING_LIST);
    private static final Set<Integer> HISTORY_SYNC_DATA_TYPES =
            Set.of(UserSelectableType.HISTORY, UserSelectableType.TABS);

    /**
     * This class is used to test {@link SyncConsentFirstRunFragment}.
     */
    public static class CustomSyncConsentFirstRunFragment extends SyncConsentFirstRunFragment {
        private FirstRunPageDelegate mFirstRunPageDelegate;
        private boolean mIsUpdateAccountCalled;

        @Override
        public FirstRunPageDelegate getPageDelegate() {
            return mFirstRunPageDelegate;
        }

        private void setPageDelegate(FirstRunPageDelegate delegate) {
            mFirstRunPageDelegate = delegate;
        }

        @Override
        protected void updateAccounts(List<Account> accounts) {
            super.updateAccounts(accounts);
            mIsUpdateAccountCalled = true;
        }
    }

    @Rule
    public final TestRule mCommandLindFlagRule = CommandLineFlags.getTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final HistogramTestRule mHistogramTestRule = new HistogramTestRule();

    @Rule
    public final ChromeTabbedActivityTestRule mChromeActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_REVISION)
                    .setDescription(RENDER_DESCRIPTION)
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @Mock
    private FirstRunPageDelegate mFirstRunPageDelegateMock;

    @Mock
    private ExternalAuthUtils mExternalAuthUtilsMock;

    private SyncConsentActivity mSyncConsentActivity;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        // Only needs to be loaded once and needs to be loaded before HistogramTestRule.
        // TODO(https://crbug.com/1211884): Revise after HistogramTestRule is revised to not require
        // native loading.
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @Before
    public void setUp() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
        mActivityTestRule.setFinishActivity(true);
        mChromeActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() throws Exception {
        // Since {@link SyncConsentActivity} is launched inside this test class, we need to
        // tear it down inside the class as well.
        if (mSyncConsentActivity != null) {
            ApplicationTestUtils.finishActivity(mSyncConsentActivity);
        }
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testSyncConsentFragmentDefaultAccount() throws IOException {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoDefaultFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER, accountInfo.getEmail());
                });
        mRenderTestRule.render(mSyncConsentActivity.findViewById(R.id.fragment_container),
                "sync_consent_fragment_default_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testTangibleSyncConsentFragmentDefaultAccount() throws IOException {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForTangibleSyncFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER, accountInfo.getEmail());
                });
        mRenderTestRule.render(mSyncConsentActivity.findViewById(R.id.fragment_container),
                "tangible_sync_consent_fragment_default_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.TANGIBLE_SYNC + ":group_id/2"})
    public void testTangibleSyncConsentFragmentVariantBDefaultAccount() throws IOException {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForTangibleSyncFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER, accountInfo.getEmail());
                });
        mRenderTestRule.render(mSyncConsentActivity.findViewById(R.id.fragment_container),
                "tangible_sync_consent_fragment_variant_b_default_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.TANGIBLE_SYNC + ":group_id/3"})
    public void testTangibleSyncConsentFragmentVariantCDefaultAccount() throws IOException {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForTangibleSyncFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER, accountInfo.getEmail());
                });
        mRenderTestRule.render(mSyncConsentActivity.findViewById(R.id.fragment_container),
                "tangible_sync_consent_fragment_variant_c_default_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.TANGIBLE_SYNC + ":group_id/4"})
    public void testTangibleSyncConsentFragmentVariantDDefaultAccount() throws IOException {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForTangibleSyncFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER, accountInfo.getEmail());
                });
        mRenderTestRule.render(mSyncConsentActivity.findViewById(R.id.fragment_container),
                "tangible_sync_consent_fragment_variant_d_default_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.TANGIBLE_SYNC + ":group_id/5"})
    public void testTangibleSyncConsentFragmentVariantEDefaultAccount() throws IOException {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForTangibleSyncFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER, accountInfo.getEmail());
                });
        mRenderTestRule.render(mSyncConsentActivity.findViewById(R.id.fragment_container),
                "tangible_sync_consent_fragment_variant_e_default_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.TANGIBLE_SYNC + ":group_id/6"})
    public void testTangibleSyncConsentFragmentVariantFDefaultAccount() throws IOException {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForTangibleSyncFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER, accountInfo.getEmail());
                });
        mRenderTestRule.render(mSyncConsentActivity.findViewById(R.id.fragment_container),
                "tangible_sync_consent_fragment_variant_f_default_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testSyncConsentFragmentNewAccount() throws IOException {
        mSigninTestRule.setResultForNextAddAccountFlow(Activity.RESULT_CANCELED, null);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoAddAccountFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER);
                });
        mRenderTestRule.render(mSyncConsentActivity.findViewById(R.id.fragment_container),
                "sync_consent_fragment_new_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testSyncConsentFragmentNotDefaultAccountWithPrimaryAccount() throws IOException {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSigninTestRule.addAccount("test.second.account@gmail.com");
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoChooseAccountFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER, accountInfo.getEmail());
                });
        mRenderTestRule.render(mSyncConsentActivity.findViewById(R.id.fragment_container),
                "sync_consent_fragment_choose_primary_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testTangibleSyncConsentFragmentNotDefaultAccountWithSecondaryAccount()
            throws IOException {
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        String secondAccountName = "test.second.account@gmail.com";
        mSigninTestRule.addAccount(secondAccountName);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForTangibleSyncFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER, secondAccountName);
                });
        mRenderTestRule.render(mSyncConsentActivity.findViewById(R.id.fragment_container),
                "tangible_sync_consent_fragment_choose_secondary_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @DisabledTest(message = "crbug.com/1304737")
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testSyncConsentFragmentWithChildAccount() throws IOException {
        CoreAccountInfo accountInfo = mSigninTestRule.addChildTestAccountThenWaitForSignin();
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoDefaultFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER, accountInfo.getEmail());
                });
        mRenderTestRule.render(mSyncConsentActivity.findViewById(R.id.fragment_container),
                "sync_consent_fragment_child_account");
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    @EnableFeatures({ChromeFeatureList.HIDE_NON_DISPLAYABLE_ACCOUNT_EMAIL})
    public void testSyncConsentFragmentWithChildAccountWithNonDisplayableAccountEmail()
            throws IOException {
        CoreAccountInfo accountInfo = mSigninTestRule.addAccount(
                SigninTestRule.generateChildEmail(AccountManagerTestRule.TEST_ACCOUNT_EMAIL),
                SigninTestRule.NON_DISPLAYABLE_EMAIL_ACCOUNT_CAPABILITIES);
        mSigninTestRule.waitForSeeding();
        mSigninTestRule.waitForSignin(accountInfo);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoDefaultFlow(
                            mChromeActivityTestRule.getActivity(), SigninAccessPoint.SIGNIN_PROMO,
                            accountInfo.getEmail());
                });
        onView(withText(accountInfo.getEmail())).check(doesNotExist());
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    @EnableFeatures({ChromeFeatureList.HIDE_NON_DISPLAYABLE_ACCOUNT_EMAIL})
    public void
    testSyncConsentFragmentWithChildAccountWithNonDisplayableAccountEmailWithEmptyDisplayName()
            throws IOException {
        CoreAccountInfo accountInfo = mSigninTestRule.addAccount(
                SigninTestRule.generateChildEmail(AccountManagerTestRule.TEST_ACCOUNT_EMAIL), "",
                "", null, SigninTestRule.NON_DISPLAYABLE_EMAIL_ACCOUNT_CAPABILITIES);
        mSigninTestRule.waitForSeeding();
        mSigninTestRule.waitForSignin(accountInfo);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoDefaultFlow(
                            mChromeActivityTestRule.getActivity(), SigninAccessPoint.SIGNIN_PROMO,
                            accountInfo.getEmail());
                });
        onView(withText(accountInfo.getEmail())).check(doesNotExist());
        onView(withText(R.string.default_google_account_username)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testFRESyncConsentFragmentWithNoAccountsOnDevice() throws IOException {
        var startPageHistogram = HistogramWatcher.newSingleRecordWatcher(
                "Signin.SigninStartedAccessPoint", SigninAccessPoint.START_PAGE);
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, false);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);

        launchActivityWithFragment(fragment);
        startPageHistogram.assertExpected();
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "fre_sync_consent_fragment_with_no_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testFRESyncConsentFragmentWithAdultAccount() throws IOException {
        var startPageHistogram = HistogramWatcher.newSingleRecordWatcher(
                "Signin.SigninStartedAccessPoint", SigninAccessPoint.START_PAGE);
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, false);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);

        launchActivityWithFragment(fragment);
        startPageHistogram.assertExpected();
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "fre_sync_consent_fragment_with_adult_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @EnableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testFRETangibleSyncConsentFragmentWithAdultAccount() throws IOException {
        var startPageHistogram = HistogramWatcher.newSingleRecordWatcher(
                "Signin.SigninStartedAccessPoint", SigninAccessPoint.START_PAGE);
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, false);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);

        launchActivityWithFragment(fragment);
        startPageHistogram.assertExpected();
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "fre_tangible_sync_consent_fragment_with_adult_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testFRESyncConsentFragmentWithChildAccount() throws IOException {
        var startPageHistogram = HistogramWatcher.newSingleRecordWatcher(
                "Signin.SigninStartedAccessPoint", SigninAccessPoint.START_PAGE);
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, true);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);

        launchActivityWithFragment(fragment);
        startPageHistogram.assertExpected();
        // TODO(https://crbug.com/1291903): Rewrite this test when RenderTestRule is integrated with
        // Espresso.
        // We check the button is enabled rather than visible, as it may be off-screen on small
        // devices.
        onView(withId(R.id.positive_button)).check(matches(isEnabled()));
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "fre_sync_consent_fragment_with_regular_child_allow_sync_off");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @CommandLineFlags.Remove({ChromeSwitches.FORCE_ENABLE_SIGNIN_FRE})
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testFRESyncConsentFragmentWithChildAccountLegacy() throws IOException {
        var startPageHistogram = HistogramWatcher.newSingleRecordWatcher(
                "Signin.SigninStartedAccessPoint", SigninAccessPoint.START_PAGE);
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, true);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);

        launchActivityWithFragment(fragment);
        startPageHistogram.assertExpected();
        // TODO(https://crbug.com/1291903): Rewrite this test when RenderTestRule is integrated with
        // Espresso.
        // We check the button is enabled rather than visible, as it may be off-screen on small
        // devices.
        onView(withId(R.id.positive_button)).check(matches(isEnabled()));
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "fre_sync_consent_fragment_with_regular_child_legacy");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testFRESyncConsentFragmentWhenSignedInWithoutSync() throws IOException {
        mSigninTestRule.addTestAccountThenSignin();
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, false);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);

        launchActivityWithFragment(fragment);
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "fre_sync_consent_fragment_when_signed_in_without_sync");
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testFRESyncConsentFragmentWhenSelectedAccountIsRemoved() {
        final CoreAccountInfo defaultAccount =
                mSigninTestRule.addAccount("test.default.account@gmail.com");
        final CoreAccountInfo primaryAccount = mSigninTestRule.addTestAccountThenSignin();
        assertNotEquals(
                "Primary account should be a different account!", defaultAccount, primaryAccount);
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, false);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);
        launchActivityWithFragment(fragment);

        mSigninTestRule.removeAccountAndWaitForSeeding(primaryAccount.getEmail());

        CriteriaHelper.pollUiThread(() -> fragment.mIsUpdateAccountCalled);
        verify(mFirstRunPageDelegateMock).abortFirstRunExperience();
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testFRESyncConsentFragmentWhenSignedInWithoutSyncDynamically() throws IOException {
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, false);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);
        launchActivityWithFragment(fragment);

        mSigninTestRule.addTestAccountThenSignin();

        CriteriaHelper.pollUiThread(() -> {
            return !mActivityTestRule.getActivity()
                            .findViewById(R.id.signin_account_picker)
                            .isShown();
        });
        mRenderTestRule.render(mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "fre_sync_consent_fragment_when_signed_in_without_sync_dynamically");
    }

    @Test
    @LargeTest
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testClickingSettingsDoesNotSetFirstSetupComplete() {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoDefaultFlow(
                            mChromeActivityTestRule.getActivity(), SigninAccessPoint.SETTINGS,
                            accountInfo.getEmail());
                });
        onView(withText(accountInfo.getEmail())).check(matches(isDisplayed()));
        onView(withId(R.id.signin_details_description)).perform(ViewUtils.clickOnClickableSpan(0));
        // Wait for sync opt-in process to finish.
        CriteriaHelper.pollUiThread(() -> {
            return IdentityServicesProvider.get()
                    .getSigninManager(Profile.getLastUsedRegularProfile())
                    .getIdentityManager()
                    .hasPrimaryAccount(ConsentLevel.SYNC);
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(SyncService.get().isSyncRequested());
            assertFalse(SyncService.get().isFirstSetupComplete());
            assertEquals(ALL_CLANK_SYNCABLE_DATA_TYPES, SyncService.get().getSelectedTypes());
            assertTrue(SyncService.get().hasKeepEverythingSynced());
        });
        // Close the SettingsActivity.
        onView(withId(R.id.cancel_button)).perform(click());
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testClickingSettingsDoesNotSetFirstSetupCompleteWithTangibleSync() {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForTangibleSyncFlow(
                            mChromeActivityTestRule.getActivity(), SigninAccessPoint.SETTINGS,
                            accountInfo.getEmail());
                });
        onView(withId(R.id.sync_consent_details_description))
                .perform(ViewUtils.clickOnClickableSpan(0));
        // Wait for sync opt-in process to finish.
        CriteriaHelper.pollUiThread(() -> {
            return IdentityServicesProvider.get()
                    .getSigninManager(Profile.getLastUsedRegularProfile())
                    .getIdentityManager()
                    .hasPrimaryAccount(ConsentLevel.SYNC);
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(SyncService.get().isSyncRequested());
            assertFalse(SyncService.get().isFirstSetupComplete());
            assertEquals(HISTORY_SYNC_DATA_TYPES, SyncService.get().getSelectedTypes());
            assertFalse(SyncService.get().hasKeepEverythingSynced());
        });
        // Close the SettingsActivity.
        onView(withId(R.id.cancel_button)).perform(click());
    }

    @Test
    @LargeTest
    public void testClickingSettingsThenCancelForChildIsNoOp() {
        CoreAccountInfo accountInfo = mSigninTestRule.addChildTestAccountThenWaitForSignin();
        // Check the user is not consented to sync.
        CriteriaHelper.pollUiThread(() -> {
            return !IdentityServicesProvider.get()
                            .getSigninManager(Profile.getLastUsedRegularProfile())
                            .getIdentityManager()
                            .hasPrimaryAccount(ConsentLevel.SYNC);
        });
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoDefaultFlow(
                            mChromeActivityTestRule.getActivity(), SigninAccessPoint.SETTINGS,
                            accountInfo.getEmail());
                });
        onView(withId(R.id.signin_details_description)).perform(ViewUtils.clickOnClickableSpan(0));
        // Wait for the sync consent to be set.
        CriteriaHelper.pollUiThread(() -> {
            return IdentityServicesProvider.get()
                    .getSigninManager(Profile.getLastUsedRegularProfile())
                    .getIdentityManager()
                    .hasPrimaryAccount(ConsentLevel.SYNC);
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertTrue(SyncService.get().isSyncRequested());
            assertFalse(SyncService.get().isFirstSetupComplete());
        });
        // Click the cancel button to exit the activity.
        onView(withId(R.id.cancel_button)).perform(click());
        // Check that the sync consent has been cleared (but the user is still signed in), and that
        // the sync service state changes have been undone.
        CriteriaHelper.pollUiThread(() -> {
            IdentityManager identityManager =
                    IdentityServicesProvider.get()
                            .getSigninManager(Profile.getLastUsedRegularProfile())
                            .getIdentityManager();
            return identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)
                    && !identityManager.hasPrimaryAccount(ConsentLevel.SYNC);
        });
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testTangibleSyncConsentFragmentWhenSelectedAccountIsRemoved() {
        mSigninTestRule.addAccount("test.default.account@gmail.com");
        CoreAccountInfo selectedAccountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForTangibleSyncFlow(
                            mChromeActivityTestRule.getActivity(), SigninAccessPoint.SETTINGS,
                            selectedAccountInfo.getEmail());
                });

        mSigninTestRule.removeAccount(selectedAccountInfo.getEmail());

        ApplicationTestUtils.waitForActivityState(mSyncConsentActivity, Stage.DESTROYED);
    }

    @Test
    @LargeTest
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testFRESyncConsentFragmentWithoutSelectedAccount() {
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, false);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);
        launchActivityWithFragment(fragment);

        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);

        onView(withText(R.string.signin_account_picker_dialog_title)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testSyncConsentFragmentWithDefaultFlow() {
        var settingsHistogram = HistogramWatcher.newSingleRecordWatcher(
                "Signin.SigninStartedAccessPoint", SigninAccessPoint.SETTINGS);
        mSigninTestRule.setResultForNextAddAccountFlow(Activity.RESULT_CANCELED, null);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoAddAccountFlow(
                            mChromeActivityTestRule.getActivity(), SigninAccessPoint.SETTINGS);
                });
        onView(withId(R.id.positive_button)).check(matches(withText(R.string.signin_add_account)));
        onView(withId(R.id.negative_button)).check(matches(withText(R.string.cancel)));
        settingsHistogram.assertExpected();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals(ALL_CLANK_SYNCABLE_DATA_TYPES, SyncService.get().getSelectedTypes());
            assertTrue(SyncService.get().hasKeepEverythingSynced());
        });
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testSelectNonDefaultAccountInAccountPickerDialog() {
        var bookmarkHistogram = HistogramWatcher.newSingleRecordWatcher(
                "Signin.SigninStartedAccessPoint", SigninAccessPoint.BOOKMARK_MANAGER);
        CoreAccountInfo defaultAccountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        String nonDefaultAccountName = "test.account.nondefault@gmail.com";
        mSigninTestRule.addAccount(nonDefaultAccountName);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoDefaultFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER, defaultAccountInfo.getEmail());
                });
        onView(withText(defaultAccountInfo.getEmail()))
                .check(matches(isDisplayed()))
                .perform(click());
        onView(withText(nonDefaultAccountName)).inRoot(isDialog()).perform(click());
        // We should return to the signin promo screen where the previous account is
        // not shown anymore.
        onView(withText(defaultAccountInfo.getEmail())).check(doesNotExist());
        onView(withText(nonDefaultAccountName)).check(matches(isDisplayed()));
        bookmarkHistogram.assertExpected();
    }

    @Test
    @LargeTest
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testSyncConsentFragmentAddAccountFlowSucceeded() {
        mSigninTestRule.setResultForNextAddAccountFlow(Activity.RESULT_OK, NEW_ACCOUNT_NAME);

        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoAddAccountFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER);
                });

        onView(withText(NEW_ACCOUNT_NAME)).check(matches(isDisplayed()));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        "Signin.AddAccountState", State.REQUESTED));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount("Signin.AddAccountState", State.STARTED));
        // Poll for this histogram as it's recorded asynchronously.
        CriteriaHelper.pollUiThread(() -> {
            return mHistogramTestRule.getHistogramValueCount(
                           "Signin.AddAccountState", State.SUCCEEDED)
                    == 1;
        });
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testTangibleSyncConsentFragmentAddAccountFlowSucceeded() {
        mSigninTestRule.setResultForNextAddAccountFlow(Activity.RESULT_OK, NEW_ACCOUNT_NAME);

        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get()
                            .launchActivityForTangibleSyncAddAccountFlow(
                                    mChromeActivityTestRule.getActivity(),
                                    SigninAccessPoint.BOOKMARK_MANAGER);
                });

        // Wait for the added account to be visible.
        onView(withId(R.id.sync_consent_title)).check(matches(isDisplayed()));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        "Signin.AddAccountState", State.REQUESTED));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount("Signin.AddAccountState", State.STARTED));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        "Signin.AddAccountState", State.SUCCEEDED));
        assertEquals(3, mHistogramTestRule.getHistogramTotalCount("Signin.AddAccountState"));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testTangibleSyncConsentFragmentOnlyEnablesSpecificDataTypes() {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForTangibleSyncFlow(
                            mChromeActivityTestRule.getActivity(), SigninAccessPoint.SETTINGS,
                            accountInfo.getEmail());
                });
        onView(withId(R.id.positive_button)).perform(click());
        // Wait for sync opt-in process to finish.
        CriteriaHelper.pollUiThread(() -> {
            return IdentityServicesProvider.get()
                    .getSigninManager(Profile.getLastUsedRegularProfile())
                    .getIdentityManager()
                    .hasPrimaryAccount(ConsentLevel.SYNC);
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals(HISTORY_SYNC_DATA_TYPES, SyncService.get().getSelectedTypes());
            assertFalse(SyncService.get().hasKeepEverythingSynced());
        });
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TANGIBLE_SYNC + ":group_id/6"})
    public void testTangibleSyncConsentFragmentGroupFEnablesAllDataTypes() {
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForTangibleSyncFlow(
                            mChromeActivityTestRule.getActivity(), SigninAccessPoint.SETTINGS,
                            accountInfo.getEmail());
                });
        onView(withId(R.id.positive_button)).perform(click());
        // Wait for sync opt-in process to finish.
        CriteriaHelper.pollUiThread(() -> {
            return IdentityServicesProvider.get()
                    .getSigninManager(Profile.getLastUsedRegularProfile())
                    .getIdentityManager()
                    .hasPrimaryAccount(ConsentLevel.SYNC);
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals(ALL_CLANK_SYNCABLE_DATA_TYPES, SyncService.get().getSelectedTypes());
            assertTrue(SyncService.get().hasKeepEverythingSynced());
        });
    }

    @Test
    @LargeTest
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testSyncConsentFragmentAddAccountFlowCancelled() {
        mSigninTestRule.setResultForNextAddAccountFlow(Activity.RESULT_CANCELED, null);

        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoAddAccountFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER);
                });

        onView(withText(R.string.signin_add_account)).check(matches(isDisplayed()));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        "Signin.AddAccountState", State.REQUESTED));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount("Signin.AddAccountState", State.STARTED));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        "Signin.AddAccountState", State.CANCELLED));
        assertEquals(3, mHistogramTestRule.getHistogramTotalCount("Signin.AddAccountState"));
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testTangibleSyncConsentFragmentAddAccountFlowCancelled() {
        mSigninTestRule.setResultForNextAddAccountFlow(Activity.RESULT_CANCELED, null);

        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get()
                            .launchActivityForTangibleSyncAddAccountFlow(
                                    mChromeActivityTestRule.getActivity(),
                                    SigninAccessPoint.BOOKMARK_MANAGER);
                });

        // SyncConsentActivity is destroyed if add account flow is cancelled.
        ApplicationTestUtils.waitForActivityState(mSyncConsentActivity, Stage.DESTROYED);
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        "Signin.AddAccountState", State.REQUESTED));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount("Signin.AddAccountState", State.STARTED));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        "Signin.AddAccountState", State.CANCELLED));
        assertEquals(3, mHistogramTestRule.getHistogramTotalCount("Signin.AddAccountState"));
    }

    @Test
    @LargeTest
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testSyncConsentFragmentAddAccountFlowFailed() {
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoAddAccountFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER);
                });

        // In this case the sync consent activity will be backgrounded and android settings page
        // will be shown.
        ApplicationTestUtils.waitForActivityState(mSyncConsentActivity, Stage.STOPPED);
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        "Signin.AddAccountState", State.REQUESTED));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount("Signin.AddAccountState", State.FAILED));
        assertEquals(2, mHistogramTestRule.getHistogramTotalCount("Signin.AddAccountState"));
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testTangibleSyncConsentFragmentAddAccountFlowFailed() {
        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get()
                            .launchActivityForTangibleSyncAddAccountFlow(
                                    mChromeActivityTestRule.getActivity(),
                                    SigninAccessPoint.BOOKMARK_MANAGER);
                });

        // SyncConsentActivity is destroyed if add account flow fails.
        ApplicationTestUtils.waitForActivityState(mSyncConsentActivity, Stage.DESTROYED);
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        "Signin.AddAccountState", State.REQUESTED));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount("Signin.AddAccountState", State.FAILED));
        assertEquals(2, mHistogramTestRule.getHistogramTotalCount("Signin.AddAccountState"));
    }

    @Test
    @LargeTest
    @DisableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testSyncConsentFragmentAddAccountFlowReturnedNullAccountName() {
        mSigninTestRule.setResultForNextAddAccountFlow(Activity.RESULT_OK, null);

        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get().launchActivityForPromoAddAccountFlow(
                            mChromeActivityTestRule.getActivity(),
                            SigninAccessPoint.BOOKMARK_MANAGER);
                });

        onView(withText(R.string.signin_add_account)).check(matches(isDisplayed()));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        "Signin.AddAccountState", State.REQUESTED));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount("Signin.AddAccountState", State.STARTED));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        "Signin.AddAccountState", State.SUCCEEDED));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        "Signin.AddAccountState", State.NULL_ACCOUNT_NAME));
        assertEquals(4, mHistogramTestRule.getHistogramTotalCount("Signin.AddAccountState"));
    }

    @Test
    @LargeTest
    @EnableFeatures({ChromeFeatureList.TANGIBLE_SYNC})
    public void testTangibleSyncConsentFragmentAddAccountFlowReturnedNullAccountName() {
        mSigninTestRule.setResultForNextAddAccountFlow(Activity.RESULT_OK, null);

        mSyncConsentActivity = ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(), SyncConsentActivity.class, () -> {
                    SyncConsentActivityLauncherImpl.get()
                            .launchActivityForTangibleSyncAddAccountFlow(
                                    mChromeActivityTestRule.getActivity(),
                                    SigninAccessPoint.BOOKMARK_MANAGER);
                });

        // SyncConsentActivity is destroyed if the add account flow returns null account name.
        ApplicationTestUtils.waitForActivityState(mSyncConsentActivity, Stage.DESTROYED);
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        "Signin.AddAccountState", State.REQUESTED));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount("Signin.AddAccountState", State.STARTED));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        "Signin.AddAccountState", State.SUCCEEDED));
        assertEquals(1,
                mHistogramTestRule.getHistogramValueCount(
                        "Signin.AddAccountState", State.NULL_ACCOUNT_NAME));
        assertEquals(4, mHistogramTestRule.getHistogramTotalCount("Signin.AddAccountState"));
    }

    private void launchActivityWithFragment(Fragment fragment) {
        mActivityTestRule.launchActivity(null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity()
                    .getSupportFragmentManager()
                    .beginTransaction()
                    .add(android.R.id.content, fragment)
                    .commit();
        });
        ApplicationTestUtils.waitForActivityState(mActivityTestRule.getActivity(), Stage.RESUMED);
    }
}
