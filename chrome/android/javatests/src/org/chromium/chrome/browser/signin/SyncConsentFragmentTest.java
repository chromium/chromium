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
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.runner.lifecycle.Stage;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags.Add;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.firstrun.FirstRunPageDelegate;
import org.chromium.chrome.browser.firstrun.SyncConsentFirstRunFragment;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.signin.services.SigninMetricsUtils.State;
import org.chromium.chrome.browser.ui.signin.MinorModeHelper;
import org.chromium.chrome.test.AutomotiveContextWrapperTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.IdentityManagerJni;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.metrics.SyncButtonClicked;
import org.chromium.components.signin.metrics.SyncButtonsType;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;
import java.util.List;
import java.util.Set;

/** Render tests for sync consent fragment. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS)
@DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
public class SyncConsentFragmentTest {
    private static final int RENDER_REVISION = 3;
    private static final String RENDER_DESCRIPTION = "Change button style";
    private static final String NEW_ACCOUNT_NAME = "new.account@gmail.com";
    private static final Set<Integer> ALL_CLANK_SYNCABLE_DATA_TYPES =
            Set.of(
                    UserSelectableType.AUTOFILL,
                    UserSelectableType.PAYMENTS,
                    UserSelectableType.BOOKMARKS,
                    UserSelectableType.PASSWORDS,
                    UserSelectableType.PREFERENCES,
                    UserSelectableType.TABS,
                    UserSelectableType.HISTORY,
                    UserSelectableType.READING_LIST);

    /** This class is used to test {@link SyncConsentFirstRunFragment}. */
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
        protected void updateAccounts(List<CoreAccountInfo> coreAccountInfos) {
            super.updateAccounts(coreAccountInfos);
            mIsUpdateAccountCalled = true;
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public AutomotiveContextWrapperTestRule mAutoTestRule = new AutomotiveContextWrapperTestRule();

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

    @Rule public final JniMocker mocker = new JniMocker();

    @Mock private IdentityManager.Natives mIdentityManagerNativeMock;

    @Mock private FirstRunPageDelegate mFirstRunPageDelegateMock;

    @Mock private ExternalAuthUtils mExternalAuthUtilsMock;

    // Needed so proguard doesn't mess up mocking.
    @Mock private SigninManagerImpl mUnused;

    private SyncConsentActivity mSyncConsentActivity;

    @Before
    public void setUp() {
        when(mExternalAuthUtilsMock.canUseGooglePlayServices(any())).thenReturn(true);

        OneshotSupplier<ProfileProvider> profileProviderSupplier =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            OneshotSupplierImpl<ProfileProvider> supplierImpl =
                                    new OneshotSupplierImpl<>();
                            supplierImpl.set(
                                    new ProfileProvider() {
                                        @NonNull
                                        @Override
                                        public Profile getOriginalProfile() {
                                            return ProfileManager.getLastUsedRegularProfile();
                                        }

                                        @Nullable
                                        @Override
                                        public Profile getOffTheRecordProfile(
                                                boolean createIfNeeded) {
                                            return null;
                                        }

                                        @Override
                                        public boolean hasOffTheRecordProfile() {
                                            return false;
                                        }
                                    });
                            return supplierImpl;
                        });
        when(mFirstRunPageDelegateMock.getProfileProviderSupplier())
                .thenReturn(profileProviderSupplier);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtilsMock);
        mActivityTestRule.setFinishActivity(true);
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
    public void testSyncConsentFragmentDefaultAccount() throws IOException {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoDefaultFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.BOOKMARK_MANAGER,
                                            AccountManagerTestRule.AADC_ADULT_ACCOUNT.getEmail());
                        });

        mRenderTestRule.render(
                mSyncConsentActivity.findViewById(R.id.fragment_container),
                "sync_consent_fragment_default_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncConsentFragmentMinorAwareDefaultAccount() throws IOException {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoDefaultFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.BOOKMARK_MANAGER,
                                            AccountManagerTestRule.AADC_MINOR_ACCOUNT.getEmail());
                        });
        mRenderTestRule.render(
                mSyncConsentActivity.findViewById(R.id.fragment_container),
                "sync_consent_fragment_minor_aware_default_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncConsentFragmentNewAccount() throws IOException {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoAddAccountFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.BOOKMARK_MANAGER);
                            onViewWaiting(AccountManagerTestRule.CANCEL_ADD_ACCOUNT_BUTTON_MATCHER)
                                    .perform(click());
                        });
        mRenderTestRule.render(
                mSyncConsentActivity.findViewById(R.id.fragment_container),
                "sync_consent_fragment_new_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncConsentFragmentNotDefaultAccountWithPrimaryAccount() throws IOException {
        mChromeActivityTestRule.startMainActivityOnBlankPage();

        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);

        // Resolve minor mode of TEST_ACCOUNT_1 before taking screenshot.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSigninTestRule.resolveMinorModeToUnrestricted(
                            AccountManagerTestRule.TEST_ACCOUNT_1.getId());
                });

        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoChooseAccountFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.BOOKMARK_MANAGER,
                                            AccountManagerTestRule.AADC_ADULT_ACCOUNT.getEmail());
                        });
        mRenderTestRule.render(
                mSyncConsentActivity.findViewById(R.id.fragment_container),
                "sync_consent_fragment_choose_primary_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @DisabledTest(message = "crbug.com/1304737")
    public void testSyncConsentFragmentWithChildAccount() throws IOException {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        CoreAccountInfo accountInfo = mSigninTestRule.addChildTestAccountThenWaitForSignin();
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoDefaultFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.BOOKMARK_MANAGER,
                                            accountInfo.getEmail());
                            onViewWaiting(AccountManagerTestRule.CANCEL_ADD_ACCOUNT_BUTTON_MATCHER)
                                    .perform(click());
                        });
        mRenderTestRule.render(
                mSyncConsentActivity.findViewById(R.id.fragment_container),
                "sync_consent_fragment_child_account");
    }

    @Test
    @MediumTest
    public void testSyncConsentFragmentWithChildAccountWithNonDisplayableAccountEmail()
            throws IOException {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        AccountInfo accountInfo = AccountManagerTestRule.TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL;
        mSigninTestRule.addAccount(accountInfo);
        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoDefaultFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.SIGNIN_PROMO,
                                            accountInfo.getEmail());
                        });
        onView(withText(accountInfo.getEmail())).check(doesNotExist());
    }

    @Test
    @MediumTest
    public void
            testSyncConsentFragmentWithChildAccountWithNonDisplayableAccountEmailWithEmptyDisplayName()
                    throws IOException {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        AccountInfo accountInfo =
                AccountManagerTestRule.TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL_AND_NO_NAME;
        mSigninTestRule.addAccountThenSignin(accountInfo);

        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoDefaultFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.SIGNIN_PROMO,
                                            accountInfo.getEmail());
                        });
        onView(withText(accountInfo.getEmail())).check(doesNotExist());
        onView(withText(R.string.default_google_account_username)).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    @DisableIf.Build(
            sdk_is_less_than = VERSION_CODES.P,
            message = "Flaky on Oreo. See crbug.com/41493567")
    public void testFRESyncConsentFragmentWithNoAccountsOnDevice() throws IOException {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        var startPageHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SigninStartedAccessPoint", SigninAccessPoint.START_PAGE);
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, false);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);

        launchActivityWithFragment(fragment);
        startPageHistogram.assertExpected();
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "fre_sync_consent_fragment_with_no_account");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testFRESyncConsentFragmentWithAdultAccount() throws IOException {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        var startPageHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SigninStartedAccessPoint", SigninAccessPoint.START_PAGE);
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, false);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);

        launchActivityWithFragment(fragment);
        startPageHistogram.assertExpected();
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "fre_sync_consent_fragment_with_adult_account");
    }

    /**
     * This test is using default account turned into child account.
     *
     * <p>Default accounts do not specify minor-mode restrictions and as a consequence, wait for
     * them to be resolved to either minor-restricted or -unrestricted mode.
     */
    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testFRESyncConsentFragmentWithChildAccount() throws IOException {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        var startPageHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SigninStartedAccessPoint", SigninAccessPoint.START_PAGE);
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_1);
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, true);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);

        launchActivityWithFragment(fragment);
        startPageHistogram.assertExpected();

        // Resolves minor-mode to unrestricted; so the user will experience weighted buttons.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSigninTestRule.resolveMinorModeToUnrestricted(
                            AccountManagerTestRule.TEST_ACCOUNT_1.getId());
                });
        ViewUtils.waitForVisibleView(withText(R.string.signin_accept_button));

        // TODO(crbug.com/40212926): Rewrite this test when RenderTestRule is integrated with
        // Espresso.
        // We check the button is enabled rather than visible, as it may be off-screen on small
        // devices.
        onView(withId(R.id.button_primary)).check(matches(isEnabled()));
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "fre_sync_consent_fragment_with_regular_child_allow_sync_off");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testFRESyncConsentFragmentWithChildAccountWithMinorModeRestrictionsEnabled()
            throws IOException {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        var startPageHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SigninStartedAccessPoint", SigninAccessPoint.START_PAGE);
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, true);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);

        launchActivityWithFragment(fragment);
        startPageHistogram.assertExpected();
        // TODO(crbug.com/40212926): Rewrite this test when RenderTestRule is integrated with
        // Espresso.
        // We check the button is enabled rather than visible, as it may be off-screen on small
        // devices.
        onView(withId(R.id.button_primary)).check(matches(isEnabled()));
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "fre_sync_consent_fragment_with_regular_child_allow_sync_off_with_minor_mode_restrictions_enabled");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testFRESyncConsentFragmentWhenSignedInWithoutSync() throws IOException {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addTestAccountThenSignin();
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, false);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);

        launchActivityWithFragment(fragment);
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "fre_sync_consent_fragment_when_signed_in_without_sync");
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1449158")
    public void testFRESyncConsentFragmentWhenSelectedAccountIsRemoved() {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
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

        mSigninTestRule.removeAccount(primaryAccount.getId());

        CriteriaHelper.pollUiThread(() -> fragment.mIsUpdateAccountCalled);
        verify(mFirstRunPageDelegateMock).abortFirstRunExperience();
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testFRESyncConsentFragmentWhenSignedInWithoutSyncDynamically() throws IOException {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        CustomSyncConsentFirstRunFragment fragment = new CustomSyncConsentFirstRunFragment();
        Bundle bundle = new Bundle();
        bundle.putBoolean(SyncConsentFirstRunFragment.IS_CHILD_ACCOUNT, false);
        when(mFirstRunPageDelegateMock.getProperties()).thenReturn(bundle);
        fragment.setPageDelegate(mFirstRunPageDelegateMock);
        launchActivityWithFragment(fragment);

        mSigninTestRule.addTestAccountThenSignin();

        CriteriaHelper.pollUiThread(
                () -> {
                    return !mActivityTestRule
                            .getActivity()
                            .findViewById(R.id.signin_account_picker)
                            .isShown();
                });
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(android.R.id.content),
                "fre_sync_consent_fragment_when_signed_in_without_sync_dynamically");
    }

    @Test
    @LargeTest
    @DisabledTest(message = "crbug.com/1462981")
    public void testClickingSettingsDoesNotSetInitialSyncFeatureSetupComplete() {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoDefaultFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.SETTINGS,
                                            accountInfo.getEmail());
                        });
        onView(withText(accountInfo.getEmail())).check(matches(isDisplayed()));
        onView(withId(R.id.signin_details_description)).perform(ViewUtils.clickOnClickableSpan(0));
        // Wait for sync opt-in process to finish.
        CriteriaHelper.pollUiThread(
                () -> {
                    return IdentityServicesProvider.get()
                            .getSigninManager(ProfileManager.getLastUsedRegularProfile())
                            .getIdentityManager()
                            .hasPrimaryAccount(ConsentLevel.SYNC);
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SyncService syncService = SyncTestUtil.getSyncServiceForLastUsedProfile();
                    assertTrue(syncService.hasSyncConsent());
                    assertFalse(syncService.isInitialSyncFeatureSetupComplete());
                    assertEquals(ALL_CLANK_SYNCABLE_DATA_TYPES, syncService.getSelectedTypes());
                    assertTrue(syncService.hasKeepEverythingSynced());
                });
        // Close the SettingsActivity.
        onView(withId(R.id.cancel_button)).perform(click());
    }


    @Test
    @LargeTest
    public void testClickingSettingsThenCancelForChildIsNoOp() {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        CoreAccountInfo accountInfo = mSigninTestRule.addChildTestAccountThenWaitForSignin();
        // Check the user is not consented to sync.
        CriteriaHelper.pollUiThread(
                () -> {
                    return !IdentityServicesProvider.get()
                            .getSigninManager(ProfileManager.getLastUsedRegularProfile())
                            .getIdentityManager()
                            .hasPrimaryAccount(ConsentLevel.SYNC);
                });
        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoDefaultFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.SETTINGS,
                                            accountInfo.getEmail());
                        });
        onView(withId(R.id.signin_details_description)).perform(ViewUtils.clickOnClickableSpan(0));
        simulateDeviceLockReadyOnAutomotive();
        // Wait for the sync consent to be set.
        CriteriaHelper.pollUiThread(
                () -> {
                    return IdentityServicesProvider.get()
                            .getSigninManager(ProfileManager.getLastUsedRegularProfile())
                            .getIdentityManager()
                            .hasPrimaryAccount(ConsentLevel.SYNC);
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SyncService syncService = SyncTestUtil.getSyncServiceForLastUsedProfile();
                    assertTrue(syncService.hasSyncConsent());
                    assertFalse(syncService.isInitialSyncFeatureSetupComplete());
                });
        onView(withId(R.id.cancel_button)).perform(click());
        // Check that the sync consent has been cleared (but the user is still signed in), and that
        // the sync service state changes have been undone.
        CriteriaHelper.pollUiThread(
                () -> {
                    IdentityManager identityManager =
                            IdentityServicesProvider.get()
                                    .getSigninManager(ProfileManager.getLastUsedRegularProfile())
                                    .getIdentityManager();
                    return identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)
                            && !identityManager.hasPrimaryAccount(ConsentLevel.SYNC);
                });
    }

    @Test
    @LargeTest
    public void testFRESyncConsentFragmentWithoutSelectedAccount() {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
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
    public void testSyncConsentFragmentWithDefaultFlow() {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        var settingsHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SigninStartedAccessPoint", SigninAccessPoint.SETTINGS);
        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoAddAccountFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.SETTINGS);
                            onViewWaiting(AccountManagerTestRule.CANCEL_ADD_ACCOUNT_BUTTON_MATCHER)
                                    .perform(click());
                        });
        onView(withId(R.id.button_primary)).check(matches(withText(R.string.signin_add_account)));
        onView(withId(R.id.button_secondary)).check(matches(withText(R.string.cancel)));
        settingsHistogram.assertExpected();
        // As there is no account on the device, the set of selected types will be empty. Sync Setup
        // UI in this case does not link to the types list.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SyncService syncService = SyncTestUtil.getSyncServiceForLastUsedProfile();
                    assertEquals(Set.of(), syncService.getSelectedTypes());
                    assertTrue(syncService.hasKeepEverythingSynced());
                });
    }

    @Test
    @MediumTest
    public void testSelectNonDefaultAccountInAccountPickerDialog() {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        var bookmarkHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SigninStartedAccessPoint", SigninAccessPoint.BOOKMARK_MANAGER);
        CoreAccountInfo defaultAccountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        String nonDefaultAccountName = "test.account.nondefault@gmail.com";
        mSigninTestRule.addAccount(nonDefaultAccountName);
        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoDefaultFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.BOOKMARK_MANAGER,
                                            defaultAccountInfo.getEmail());
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
    @MediumTest
    public void testSelectNonDefaultAccountInAccountPickerDialogOpposingCapability() {
        mocker.mock(IdentityManagerJni.TEST_HOOKS, mIdentityManagerNativeMock);
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_MINOR_ACCOUNT);

        when(mIdentityManagerNativeMock.findExtendedAccountInfoByEmailAddress(
                        anyLong(), eq(AccountManagerTestRule.AADC_ADULT_ACCOUNT.getEmail())))
                .thenReturn(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        when(mIdentityManagerNativeMock.findExtendedAccountInfoByEmailAddress(
                        anyLong(), eq(AccountManagerTestRule.AADC_MINOR_ACCOUNT.getEmail())))
                .thenReturn(AccountManagerTestRule.AADC_MINOR_ACCOUNT);

        mSyncConsentActivity =
                waitForSyncConsentActivity(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        ViewUtils.waitForVisibleView(withText(R.string.signin_accept_button));

        // Default account has the capability MINOR_MODE_NOT_REQUIRED thus buttons will be unequally
        // weighted
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Button primaryButton = mSyncConsentActivity.findViewById(R.id.button_primary);
                    Button secondaryButton =
                            mSyncConsentActivity.findViewById(R.id.button_secondary);
                    Assert.assertEquals(View.VISIBLE, primaryButton.getVisibility());
                    Assert.assertEquals(View.VISIBLE, secondaryButton.getVisibility());
                    Assert.assertNotEquals(
                            primaryButton.getTextColors().getDefaultColor(),
                            secondaryButton.getTextColors().getDefaultColor());
                });

        onView(withText(AccountManagerTestRule.AADC_ADULT_ACCOUNT.getEmail()))
                .check(matches(isDisplayed()))
                .perform(click());
        onView(withText(AccountManagerTestRule.AADC_MINOR_ACCOUNT.getEmail()))
                .inRoot(isDialog())
                .perform(click());
        ViewUtils.waitForVisibleView(withText(R.string.signin_accept_button));

        // Sync consent activity now has the non-default account which has the capability
        // MINOR_MODE_REQUIRED thus buttons will be equally weighted
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Button primaryButton = mSyncConsentActivity.findViewById(R.id.button_primary);
                    Button secondaryButton =
                            mSyncConsentActivity.findViewById(R.id.button_secondary);
                    Assert.assertEquals(View.VISIBLE, primaryButton.getVisibility());
                    Assert.assertEquals(View.VISIBLE, secondaryButton.getVisibility());
                    Assert.assertEquals(
                            primaryButton.getTextColors().getDefaultColor(),
                            secondaryButton.getTextColors().getDefaultColor());
                });
    }

    @Test
    @LargeTest
    public void testSyncConsentFragmentAddAccountFlowSucceeded() {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        HistogramWatcher addAccountStateHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AddAccountState",
                                State.REQUESTED,
                                State.STARTED,
                                State.SUCCEEDED)
                        .build();

        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoAddAccountFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.BOOKMARK_MANAGER);
                            mSigninTestRule.setAddAccountFlowResult(NEW_ACCOUNT_NAME);
                            onViewWaiting(AccountManagerTestRule.ADD_ACCOUNT_BUTTON_MATCHER)
                                    .perform(click());
                        });
        onView(withText(NEW_ACCOUNT_NAME)).check(matches(isDisplayed()));
        // Poll for these histograms as SUCCEEDED is recorded asynchronously.
        addAccountStateHistogram.pollInstrumentationThreadUntilSatisfied();
    }

    @Test
    @LargeTest
    public void testSyncConsentFragmentAddAccountFlowCancelled() {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        HistogramWatcher addAccountStateHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AddAccountState",
                                State.REQUESTED,
                                State.STARTED,
                                State.CANCELLED)
                        .build();

        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoAddAccountFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.BOOKMARK_MANAGER);
                            onViewWaiting(AccountManagerTestRule.CANCEL_ADD_ACCOUNT_BUTTON_MATCHER)
                                    .perform(click());
                        });
        onView(withText(R.string.signin_add_account)).check(matches(isDisplayed()));
        addAccountStateHistogram.assertExpected();
    }

    @Test
    @LargeTest
    public void testSyncConsentFragmentAddAccountFlowFailed() {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        HistogramWatcher addAccountStateHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords("Signin.AddAccountState", State.REQUESTED, State.FAILED)
                        .build();

        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            mSigninTestRule.forceAddAccountIntentCreationFailure();
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoAddAccountFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.BOOKMARK_MANAGER);
                        });

        // In this case the sync consent activity will be backgrounded and android settings page
        // will be shown.
        ApplicationTestUtils.waitForActivityState(mSyncConsentActivity, Stage.STOPPED);
        addAccountStateHistogram.assertExpected();
    }

    @Test
    @LargeTest
    public void testSyncConsentFragmentAddAccountFlowReturnedNullAccountName() {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        HistogramWatcher addAccountStateHistogram =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                "Signin.AddAccountState",
                                State.REQUESTED,
                                State.STARTED,
                                State.SUCCEEDED,
                                State.NULL_ACCOUNT_NAME)
                        .build();

        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoAddAccountFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.BOOKMARK_MANAGER);
                            onViewWaiting(AccountManagerTestRule.ADD_ACCOUNT_BUTTON_MATCHER)
                                    .perform(click());
                        });
        onView(withText(R.string.signin_add_account)).check(matches(isDisplayed()));
        addAccountStateHistogram.assertExpected();
    }

    @Test
    @LargeTest
    public void testAutomotiveDevice_deviceLockCreated_syncAcceptedSuccessfully()
            throws IOException {
        mAutoTestRule.setIsAutomotive(true);
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoDefaultFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.BOOKMARK_MANAGER,
                                            accountInfo.getEmail());
                        });

        // Should display the sync page, clicking the 'more' button to scroll down if needed.
        if (mSyncConsentActivity.findViewById(R.id.more_button).isShown()) {
            onView(withId(R.id.more_button)).perform(click());
        }

        // Minor-mode safety needs time to show buttons and is not revealing them immediately.
        ViewUtils.waitForVisibleView(withText(R.string.signin_accept_button));
        onView(withText(R.string.signin_accept_button))
                .check(matches(isDisplayed()))
                .perform(click());

        // Accepting the sync on an automotive device should take the user to the device lock page.
        ViewUtils.waitForVisibleView(withId(R.id.device_lock_title));
        onView(withText(R.string.signin_accept_button)).check(doesNotExist());

        simulateDeviceLockReadyOnAutomotive();

        // Wait for the sync consent to be set and the activity has finished.
        CriteriaHelper.pollUiThread(
                () -> {
                    return IdentityServicesProvider.get()
                            .getIdentityManager(ProfileManager.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SYNC);
                });
        onView(withId(R.id.device_lock_title)).check(doesNotExist());
        ApplicationTestUtils.waitForActivityState(mSyncConsentActivity, Stage.DESTROYED);
    }

    @Test
    @LargeTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    @DisabledTest(message = "https://crbug.com/333735758")
    public void
            testAutomotiveDevice_deviceLockCreated_syncAcceptedSuccessfully_withMinorModeRestrictionsEnabled()
                    throws IOException {
        mAutoTestRule.setIsAutomotive(true);
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoDefaultFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.BOOKMARK_MANAGER,
                                            accountInfo.getEmail());
                        });

        // Should display the sync page, clicking the 'more' button to scroll down if needed.
        if (mSyncConsentActivity.findViewById(R.id.more_button).isShown()) {
            onView(withId(R.id.more_button)).perform(click());
        }

        // The account is created without a capability that determines its minor mode, wait until
        // all buttons are created in minor safe mode.
        ViewUtils.onViewWaiting(withText(R.string.signin_accept_button)).perform(click());

        // Accepting the sync on an automotive device should take the user to the device lock page.
        ViewUtils.waitForVisibleView(withId(R.id.device_lock_title));
        onView(withText(R.string.signin_accept_button)).check(doesNotExist());

        simulateDeviceLockReadyOnAutomotive();

        // Wait for the sync consent to be set and the activity has finished.
        CriteriaHelper.pollUiThread(
                () -> {
                    return IdentityServicesProvider.get()
                            .getIdentityManager(ProfileManager.getLastUsedRegularProfile())
                            .hasPrimaryAccount(ConsentLevel.SYNC);
                });
        onView(withId(R.id.device_lock_title)).check(doesNotExist());
        ApplicationTestUtils.waitForActivityState(mSyncConsentActivity, Stage.DESTROYED);
    }

    @Test
    @LargeTest
    public void testAutomotiveDevice_deviceLockRefused_syncRefused() throws Exception {
        mAutoTestRule.setIsAutomotive(true);
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoDefaultFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.BOOKMARK_MANAGER,
                                            accountInfo.getEmail());
                        });

        // Should display the sync page, clicking the 'more' button to scroll down if needed.
        if (mSyncConsentActivity.findViewById(R.id.more_button).isShown()) {
            onView(withId(R.id.more_button)).perform(click());
        }

        // Minor-mode safety needs time to show buttons and is not revealing them immediately.
        ViewUtils.waitForVisibleView(withText(R.string.signin_accept_button));
        onView(withText(R.string.signin_accept_button))
                .check(matches(isDisplayed()))
                .perform(click());

        // Accepting the sync on an automotive device should take the user to the device lock page.
        ViewUtils.waitForVisibleView(withId(R.id.device_lock_title));
        onView(withText(R.string.signin_accept_button)).check(doesNotExist());

        simulateDeviceLockRefused();

        // Check that the user is not consented to sync and the activity has finished.
        onView(withId(R.id.device_lock_title)).check(doesNotExist());
        ApplicationTestUtils.waitForActivityState(mSyncConsentActivity, Stage.DESTROYED);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(
                            IdentityServicesProvider.get()
                                    .getIdentityManager(ProfileManager.getLastUsedRegularProfile())
                                    .hasPrimaryAccount(ConsentLevel.SYNC));
                });
    }

    @Test
    @LargeTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    @DisabledTest(message = "https://crbug.com/333735758")
    public void
            testAutomotiveDevice_deviceLockRefused_syncRefused_withMinorModeRestrictionsEnabled()
                    throws Exception {
        mAutoTestRule.setIsAutomotive(true);
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoDefaultFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.BOOKMARK_MANAGER,
                                            accountInfo.getEmail());
                        });

        // Should display the sync page, clicking the 'more' button to scroll down if needed.
        if (mSyncConsentActivity.findViewById(R.id.more_button).isShown()) {
            onView(withId(R.id.more_button)).perform(click());
        }

        ViewUtils.onViewWaiting(withText(R.string.signin_accept_button)).perform(click());

        // Accepting the sync on an automotive device should take the user to the device lock page.
        ViewUtils.waitForVisibleView(withId(R.id.device_lock_title));
        onView(withText(R.string.signin_accept_button)).check(doesNotExist());

        simulateDeviceLockRefused();

        // Check that the user is not consented to sync and the activity has finished.
        onView(withId(R.id.device_lock_title)).check(doesNotExist());
        ApplicationTestUtils.waitForActivityState(mSyncConsentActivity, Stage.DESTROYED);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertFalse(
                            IdentityServicesProvider.get()
                                    .getIdentityManager(ProfileManager.getLastUsedRegularProfile())
                                    .hasPrimaryAccount(ConsentLevel.SYNC));
                });
    }

    @Test
    @LargeTest
    @DisabledTest(message = "crbug.com/1462981")
    public void testAutomotiveDevice_tryNavigateViaClickableSpan_deviceLockCreated() {
        mAutoTestRule.setIsAutomotive(true);
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoDefaultFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.SETTINGS,
                                            accountInfo.getEmail());
                        });

        onView(withText(accountInfo.getEmail())).check(matches(isDisplayed()));

        // Should display the sync page, clicking the 'more' button to scroll down if needed.
        if (mSyncConsentActivity.findViewById(R.id.more_button).isShown()) {
            onView(withId(R.id.more_button)).perform(click());
        }

        // The account is created without a capability that determines its minor mode, wait until
        // all buttons are created in minor safe mode.
        ViewUtils.waitForVisibleView(withText(R.string.signin_accept_button));

        onView(withId(R.id.signin_details_description)).perform(ViewUtils.clickOnClickableSpan(0));
        simulateDeviceLockReadyOnAutomotive();

        // Wait for sync opt-in process to finish.
        CriteriaHelper.pollUiThread(
                () -> {
                    return IdentityServicesProvider.get()
                            .getSigninManager(ProfileManager.getLastUsedRegularProfile())
                            .getIdentityManager()
                            .hasPrimaryAccount(ConsentLevel.SYNC);
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SyncService syncService = SyncTestUtil.getSyncServiceForLastUsedProfile();
                    assertTrue(
                            "The service should have recorded user consent for sync.",
                            syncService.hasSyncConsent());
                    assertFalse(
                            "Sync feature setup should not be complete without the confirm button "
                                    + "being clicked.",
                            syncService.isInitialSyncFeatureSetupComplete());
                    assertEquals(
                            "All syncable data types should be selected by default.",
                            ALL_CLANK_SYNCABLE_DATA_TYPES,
                            syncService.getSelectedTypes());
                    assertTrue(
                            "All data types should be enabled for sync.",
                            syncService.hasKeepEverythingSynced());
                });

        // Close the SettingsActivity.
        onView(withId(R.id.cancel_button)).perform(click());
    }

    @Test
    @LargeTest
    public void testAutomotiveDevice_tryNavigateViaClickableSpan_deviceLockRefused() {
        mAutoTestRule.setIsAutomotive(true);
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        CoreAccountInfo accountInfo =
                mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_EMAIL);
        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoDefaultFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.SETTINGS,
                                            accountInfo.getEmail());
                        });

        onView(withText(accountInfo.getEmail())).check(matches(isDisplayed()));

        // Should display the sync page, clicking the 'more' button to scroll down if needed.
        if (mSyncConsentActivity.findViewById(R.id.more_button).isShown()) {
            onView(withId(R.id.more_button)).perform(click());
        }
        // The account is created without a capability that determines its minor mode, wait until
        // all buttons are created in minor safe mode.
        ViewUtils.waitForVisibleView(withText(R.string.signin_accept_button));
        onView(withId(R.id.signin_details_description)).perform(ViewUtils.clickOnClickableSpan(0));
        simulateDeviceLockRefused();

        // Check that the user is not consented to sync and the activity has finished.
        CriteriaHelper.pollUiThread(
                () -> {
                    return !IdentityServicesProvider.get()
                            .getSigninManager(ProfileManager.getLastUsedRegularProfile())
                            .getIdentityManager()
                            .hasPrimaryAccount(ConsentLevel.SYNC);
                });
        onView(withId(R.id.device_lock_title)).check(doesNotExist());
        ApplicationTestUtils.waitForActivityState(mSyncConsentActivity, Stage.DESTROYED);
    }

    @Test
    @LargeTest
    public void testSignedInWithMinorModeRequiredRecordsCancelButtonClicked() throws IOException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Signin.SyncButtons.Clicked",
                                SyncButtonClicked.SYNC_CANCEL_EQUAL_WEIGHTED)
                        .expectIntRecord(
                                "Signin.SyncButtons.Shown",
                                SyncButtonsType.SYNC_EQUAL_WEIGHTED_FROM_CAPABILITY)
                        .build();

        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        SigninTestUtil.signin(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        mSyncConsentActivity =
                waitForSyncConsentActivity(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        onViewWaiting(withText(R.string.signin_sync_decline_button)).perform(click());
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    public void testSignedInWithMinorModeNotRequiredRecordsCancelButtonClicked()
            throws IOException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Signin.SyncButtons.Clicked",
                                SyncButtonClicked.SYNC_CANCEL_NOT_EQUAL_WEIGHTED)
                        .expectIntRecord(
                                "Signin.SyncButtons.Shown", SyncButtonsType.SYNC_NOT_EQUAL_WEIGHTED)
                        .build();

        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        SigninTestUtil.signin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        mSyncConsentActivity =
                waitForSyncConsentActivity(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        onViewWaiting(withText(R.string.signin_sync_decline_button)).perform(click());
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    public void testSignedInWithMinorModeRequiredRecordsAcceptButtonClicked() throws IOException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Signin.SyncButtons.Clicked",
                                SyncButtonClicked.SYNC_OPT_IN_EQUAL_WEIGHTED)
                        .expectIntRecord(
                                "Signin.SyncButtons.Shown",
                                SyncButtonsType.SYNC_EQUAL_WEIGHTED_FROM_CAPABILITY)
                        .build();

        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        SigninTestUtil.signin(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        mSyncConsentActivity =
                waitForSyncConsentActivity(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        onViewWaiting(withText(R.string.signin_accept_button)).perform(click());
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    public void testSignedInWithMinorModeNotRequiredRecordsAcceptButtonClicked()
            throws IOException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Signin.SyncButtons.Clicked",
                                SyncButtonClicked.SYNC_OPT_IN_NOT_EQUAL_WEIGHTED)
                        .expectIntRecord(
                                "Signin.SyncButtons.Shown", SyncButtonsType.SYNC_NOT_EQUAL_WEIGHTED)
                        .build();

        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        SigninTestUtil.signin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        mSyncConsentActivity =
                waitForSyncConsentActivity(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        onViewWaiting(withText(R.string.signin_accept_button)).perform(click());
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSignedInWithMinorModeRequiredHasEqualButtons() throws IOException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Signin.SyncButtons.Shown",
                                SyncButtonsType.SYNC_EQUAL_WEIGHTED_FROM_CAPABILITY)
                        .build();

        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_MINOR_ACCOUNT);

        SigninTestUtil.signin(AccountManagerTestRule.AADC_MINOR_ACCOUNT);

        mSyncConsentActivity =
                waitForSyncConsentActivity(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        mRenderTestRule.render(
                mSyncConsentActivity.findViewById(R.id.fragment_container),
                "signed_in_with_minor_mode_required_has_equal_buttons");

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSignedInWithMinorModeNotRequiredHasWeightedButtons() throws IOException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Signin.SyncButtons.Shown", SyncButtonsType.SYNC_NOT_EQUAL_WEIGHTED)
                        .build();

        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);

        SigninTestUtil.signin(AccountManagerTestRule.AADC_ADULT_ACCOUNT);

        mSyncConsentActivity =
                waitForSyncConsentActivity(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        mRenderTestRule.render(
                mSyncConsentActivity.findViewById(R.id.fragment_container),
                "signed_in_with_minor_mode_not_required_has_weighted_buttons");
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    public void testSignedInWithMinorModeUnknownHasEqualButtonsOnDeadline() throws Exception {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Signin.SyncButtons.Shown",
                                SyncButtonsType.SYNC_EQUAL_WEIGHTED_FROM_DEADLINE)
                        .build();

        mChromeActivityTestRule.startMainActivityOnBlankPage();
        // Account Capabilities are intentionally empty.
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT);

        SigninTestUtil.signin(AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT);

        mSyncConsentActivity =
                waitForSyncConsentActivity(AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT);

        // Account with no capabilities must wait to be deadlined to show buttons.
        ViewUtils.waitForVisibleView(withText(R.string.signin_accept_button));

        checkButtonsAreEquallyWeightedandVisible();
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    public void testSignedInWithMinorModeUnknownHasNoButtonsBeforeDeadline() {
        // Disable timeouting mechanism that ultimately shows some buttons
        MinorModeHelper.disableTimeoutForTesting();
        mChromeActivityTestRule.startMainActivityOnBlankPage();

        // Account Capabilities are intentionally empty - this account is waiting for capabilities
        // to be fetched to determine buttons, but in this test they will never arrive.
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT);

        SigninTestUtil.signin(AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT);
        mSyncConsentActivity =
                waitForSyncConsentActivity(AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT);

        // Since the capabilities never arrived, buttons should be still invisible
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Button primaryButton = mSyncConsentActivity.findViewById(R.id.button_primary);
                    Button secondaryButton =
                            mSyncConsentActivity.findViewById(R.id.button_secondary);
                    Assert.assertEquals(View.GONE, primaryButton.getVisibility());
                    Assert.assertEquals(View.GONE, secondaryButton.getVisibility());
                });
    }

    @Test
    @LargeTest
    public void testSignedInWithMinorModeUnknownHasEqualButtonsBeforeDeadline()
            throws InterruptedException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Signin.SyncButtons.Shown",
                                SyncButtonsType.SYNC_EQUAL_WEIGHTED_FROM_CAPABILITY)
                        .build();
        MinorModeHelper.disableTimeoutForTesting();
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        // Account Capabilities are intentionally empty.
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT);

        SigninTestUtil.signin(AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT);
        mSyncConsentActivity =
                waitForSyncConsentActivity(AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT);

        // Capability is received as MINOR_MODE_REQUIRED after an arbitrary amount of time that is
        // less than the deadline {@link
        // org.chromium.chrome.browser.ui.signin.MinorModeHelper.CAPABILITY_TIMEOUT_MS}. Buttons
        // will be equally weighted.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSigninTestRule.resolveMinorModeToRestricted(
                            AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT.getId());
                });

        ViewUtils.waitForVisibleView(withText(R.string.signin_accept_button));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Button primaryButton = mSyncConsentActivity.findViewById(R.id.button_primary);
                    Button secondaryButton =
                            mSyncConsentActivity.findViewById(R.id.button_secondary);
                    Assert.assertEquals(View.VISIBLE, primaryButton.getVisibility());
                    Assert.assertEquals(View.VISIBLE, secondaryButton.getVisibility());
                    Assert.assertEquals(
                            primaryButton.getTextColors().getDefaultColor(),
                            secondaryButton.getTextColors().getDefaultColor());
                });

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    public void testSignedInWithMinorModeUnknownHasUnequalButtonsBeforeDeadline()
            throws InterruptedException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Signin.SyncButtons.Shown", SyncButtonsType.SYNC_NOT_EQUAL_WEIGHTED)
                        .build();

        MinorModeHelper.disableTimeoutForTesting();
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        // Account Capabilities are intentionally empty.
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT);
        SigninTestUtil.signin(AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT);

        mSyncConsentActivity =
                waitForSyncConsentActivity(AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT);

        // Capability is received as MINOR_MODE_NOT_REQUIRED after an arbitrary amount of time that
        // is less than the deadline {@link
        // org.chromium.chrome.browser.ui.signin.MinorModeHelper.CAPABILITY_TIMEOUT_MS}. Buttons
        // will be unequally weighted.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mSigninTestRule.resolveMinorModeToUnrestricted(
                            AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT.getId());
                });

        ViewUtils.waitForVisibleView(withText(R.string.signin_accept_button));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Button primaryButton = mSyncConsentActivity.findViewById(R.id.button_primary);
                    Button secondaryButton =
                            mSyncConsentActivity.findViewById(R.id.button_secondary);
                    Assert.assertEquals(View.VISIBLE, primaryButton.getVisibility());
                    Assert.assertEquals(View.VISIBLE, secondaryButton.getVisibility());
                    Assert.assertNotEquals(
                            primaryButton.getTextColors().getDefaultColor(),
                            secondaryButton.getTextColors().getDefaultColor());
                });

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSignedOutWithMinorModeRequiredHasEqualButtons() throws IOException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Signin.SyncButtons.Shown",
                                SyncButtonsType.SYNC_EQUAL_WEIGHTED_FROM_CAPABILITY)
                        .build();

        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        mSyncConsentActivity =
                waitForSyncConsentActivity(AccountManagerTestRule.AADC_MINOR_ACCOUNT);
        mRenderTestRule.render(
                mSyncConsentActivity.findViewById(R.id.fragment_container),
                "signed_out_with_minor_mode_required_has_equal_buttons");

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSignedOutWithMinorModeNotRequiredHasWeightedButtons() throws IOException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Signin.SyncButtons.Shown", SyncButtonsType.SYNC_NOT_EQUAL_WEIGHTED)
                        .build();

        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        mSyncConsentActivity =
                waitForSyncConsentActivity(AccountManagerTestRule.AADC_ADULT_ACCOUNT);
        mRenderTestRule.render(
                mSyncConsentActivity.findViewById(R.id.fragment_container),
                "signed_out_with_minor_mode_not_required_has_weighted_buttons");

        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    public void testSignedOutWithMinorModeUnknownHasEqualButtonsOnDeadline() throws Exception {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Signin.SyncButtons.Shown",
                                SyncButtonsType.SYNC_EQUAL_WEIGHTED_FROM_DEADLINE)
                        .build();

        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mSigninTestRule.addAccount(AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT);
        mSyncConsentActivity =
                waitForSyncConsentActivity(AccountManagerTestRule.AADC_UNRESOLVED_ACCOUNT);

        // Signed out account with no capabilities must wait to be deadlined to show buttons.
        ViewUtils.waitForVisibleView(withText(R.string.signin_accept_button));

        checkButtonsAreEquallyWeightedandVisible();
        histogramWatcher.assertExpected();
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testSyncConsentFragmentNoAccount() throws IOException {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        mSyncConsentActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SyncConsentActivity.class,
                        () -> {
                            SyncConsentActivityLauncherImpl.get()
                                    .launchActivityForPromoAddAccountFlow(
                                            mChromeActivityTestRule.getActivity(),
                                            SigninAccessPoint.START_PAGE);
                            onViewWaiting(AccountManagerTestRule.CANCEL_ADD_ACCOUNT_BUTTON_MATCHER)
                                    .perform(click());
                        });
        mRenderTestRule.render(
                mSyncConsentActivity.findViewById(R.id.fragment_container),
                "sync_consent_fragment_with_no_accounts");
    }

    @Test
    @LargeTest
    @DisabledTest(message = "Broken and/or flake on different bots, see b/40944120.")
    public void testManagedAccount_confirmed() throws Exception {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        CoreAccountInfo accountInfo = mSigninTestRule.addAccount(NEW_ACCOUNT_NAME);

        SigninManager signinManager =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return IdentityServicesProvider.get()
                                    .getSigninManager(ProfileManager.getLastUsedRegularProfile());
                        });
        IdentityServicesProvider spyProvider = Mockito.spy(IdentityServicesProvider.get());
        SigninManager spySigninManager = Mockito.spy(signinManager);
        IdentityServicesProvider.setInstanceForTests(spyProvider);

        doReturn(spySigninManager).when(spyProvider).getSigninManager(any());
        doAnswer(
                        invocation -> {
                            ((Callback<Boolean>) invocation.getArgument(1)).onResult(true);
                            return null;
                        })
                .when(spySigninManager)
                .isAccountManaged(eq(accountInfo), any());

        mSyncConsentActivity = waitForSyncConsentActivity(accountInfo);
        View moreButton = mSyncConsentActivity.findViewById(R.id.more_button);
        if (moreButton != null && moreButton.isShown()) {
            onView(withId(R.id.more_button)).perform(click());
        }
        onViewWaiting(withText(R.string.signin_accept_button)).perform(click());
        onViewWaiting(withId(R.id.positive_button)).perform(click());
        // Wait for the sync consent to be set.
        CriteriaHelper.pollUiThread(
                () -> {
                    return IdentityServicesProvider.get()
                            .getSigninManager(ProfileManager.getLastUsedRegularProfile())
                            .getIdentityManager()
                            .hasPrimaryAccount(ConsentLevel.SYNC);
                });
        assertTrue(
                ThreadUtils.runOnUiThreadBlocking(signinManager::getUserAcceptedAccountManagement));
    }

    @Test
    @LargeTest
    @DisabledTest(message = "Broken and/or flake on different bots, see b/40944120.")
    public void testManagedAccount_failedSignin() throws Exception {
        mChromeActivityTestRule.startMainActivityOnBlankPage();
        CoreAccountInfo accountInfo = mSigninTestRule.addAccount(NEW_ACCOUNT_NAME);

        SigninManager signinManager =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return IdentityServicesProvider.get()
                                    .getSigninManager(ProfileManager.getLastUsedRegularProfile());
                        });
        IdentityServicesProvider spyProvider = Mockito.spy(IdentityServicesProvider.get());
        SigninManager spySigninManager = Mockito.spy(signinManager);
        IdentityServicesProvider.setInstanceForTests(spyProvider);

        doReturn(spySigninManager).when(spyProvider).getSigninManager(any());
        doAnswer(
                        invocation -> {
                            ((Callback<Boolean>) invocation.getArgument(1)).onResult(true);
                            return null;
                        })
                .when(spySigninManager)
                .isAccountManaged(eq(accountInfo), any());
        doAnswer(
                        invocation -> {
                            ((SigninManager.SignInCallback) invocation.getArgument(2))
                                    .onSignInAborted();
                            return null;
                        })
                .when(spySigninManager)
                .signinAndEnableSync(eq(accountInfo), anyInt(), any());

        mSyncConsentActivity = waitForSyncConsentActivity(accountInfo);
        View moreButton = mSyncConsentActivity.findViewById(R.id.more_button);
        if (moreButton != null && moreButton.isShown()) {
            onView(withId(R.id.more_button)).perform(click());
        }
        onViewWaiting(withText(R.string.signin_accept_button)).perform(click());
        onViewWaiting(withId(R.id.positive_button)).perform(click());
        assertFalse(
                ThreadUtils.runOnUiThreadBlocking(signinManager::getUserAcceptedAccountManagement));
    }

    private void simulateDeviceLockReadyOnAutomotive() {
        if (!BuildInfo.getInstance().isAutomotive) return;

        SyncConsentFragment syncConsentFragment =
                (SyncConsentFragment)
                        mSyncConsentActivity
                                .getSupportFragmentManager()
                                .findFragmentById(R.id.fragment_container);
        assertNotNull(
                "The SyncConsentActivity should contain the SyncConsentFragment!",
                syncConsentFragment);
        ThreadUtils.runOnUiThreadBlocking(syncConsentFragment::onDeviceLockReady);
    }

    private void simulateDeviceLockRefused() {
        SyncConsentFragment syncConsentFragment =
                (SyncConsentFragment)
                        mSyncConsentActivity
                                .getSupportFragmentManager()
                                .findFragmentById(R.id.fragment_container);
        assertNotNull(
                "The SyncConsentActivity should contain the SyncConsentFragment!",
                syncConsentFragment);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    syncConsentFragment.onDeviceLockRefused();
                    assertFalse(syncConsentFragment.getDeviceLockReadyForTesting());
                });
    }

    private void launchActivityWithFragment(Fragment fragment) {
        mActivityTestRule.launchActivity(null);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getSupportFragmentManager()
                            .beginTransaction()
                            .add(android.R.id.content, fragment)
                            .commit();
                });
        ApplicationTestUtils.waitForActivityState(mActivityTestRule.getActivity(), Stage.RESUMED);
    }

    private SyncConsentActivity waitForSyncConsentActivity(CoreAccountInfo accountInfo) {
        return ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(),
                SyncConsentActivity.class,
                () -> {
                    SyncConsentActivityLauncherImpl.get()
                            .launchActivityForPromoDefaultFlow(
                                    mChromeActivityTestRule.getActivity(),
                                    SigninAccessPoint.START_PAGE,
                                    accountInfo.getEmail());
                });
    }

    void checkButtonsAreEquallyWeightedandVisible() {
        onView(withId(R.id.button_primary)).check(matches(isDisplayed()));
        onView(withId(R.id.button_secondary)).check(matches(isDisplayed()));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Button primaryButton = mSyncConsentActivity.findViewById(R.id.button_primary);
                    Button secondaryButton =
                            mSyncConsentActivity.findViewById(R.id.button_secondary);
                    Assert.assertEquals(
                            primaryButton.getTextColors().getDefaultColor(),
                            secondaryButton.getTextColors().getDefaultColor());
                });
    }
}
