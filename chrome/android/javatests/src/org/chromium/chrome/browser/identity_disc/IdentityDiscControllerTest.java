// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.identity_disc;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anyOf;
import static org.hamcrest.Matchers.not;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.annotation.StringRes;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Assert;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameterBefore;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivity;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.FakeSyncServiceImpl;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils.SyncError;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonDataProvider;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;

/** Instrumentation test for Identity Disc. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IdentityDiscControllerTest {
    private static final String EMAIL = "email@gmail.com";
    private static final String NAME = "Email Emailson";
    private static final String FULL_NAME = NAME + ".full";

    private final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private final SigninTestRule mSigninTestRule = new SigninTestRule();

    // Mock sign-in environment needs to be destroyed after ChromeTabbedActivity in case there are
    // observers registered in the AccountManagerFacade mock.
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSigninTestRule).around(mActivityTestRule);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    private Tab mTab;

    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;
    @Mock private SigninManager mSigninManagerMock;
    @Mock private IdentityManager mIdentityManagerMock;
    @Mock private ObservableSupplier<Profile> mProfileSupplier;
    @Mock private ButtonDataProvider.ButtonDataObserver mButtonDataObserver;
    @Mock private Tracker mTracker;
    @Mock private ActivityLifecycleDispatcher mDispatcher;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PrefService prefService =
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    prefService.clearPref(Pref.SIGNIN_ALLOWED);
                });
    }

    @Test
    @MediumTest
    public void testIdentityDiscWithNavigation() {
        // User is signed in.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        ViewUtils.waitForVisibleView(allOf(withId(R.id.optional_toolbar_button), isDisplayed()));

        // Identity Disc should be hidden on navigation away from NTP.
        leaveNtp();
        onView(withId(R.id.optional_toolbar_button))
                .check(
                        matches(
                                anyOf(
                                        withEffectiveVisibility(ViewMatchers.Visibility.GONE),
                                        not(
                                                withContentDescription(
                                                        R.string
                                                                .accessibility_toolbar_btn_identity_disc)))));
    }

    @Test
    @MediumTest
    public void testIdentityDiscSignedOut() throws Exception {
        // When user is signed out, a signed-out avatar should be visible on the NTP.
        @StringRes int descriptionId = R.string.accessibility_toolbar_btn_signed_out_identity_disc;
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        withContentDescription(descriptionId)));

        // Clicking the signed-out avatar should lead to the correct sign-in screen.
        Activity signinActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SigninAndHistorySyncActivity.class,
                        () -> onView(withId(R.id.optional_toolbar_button)).perform(click()));
        if (signinActivity != null) {
            ApplicationTestUtils.finishActivity(signinActivity);
        }
    }

    @Test
    @MediumTest
    public void testIdentityDiscSignedOut_signinDisabled() {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    when(mIdentityServicesProviderMock.getSigninManager(Mockito.any()))
                            .thenReturn(mSigninManagerMock);
                    // This mock is required because the MainSettings class calls the
                    // IdentityManager.
                    when(mIdentityServicesProviderMock.getIdentityManager(Mockito.any()))
                            .thenReturn(mIdentityManagerMock);
                    PrefService prefService =
                            UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
                    prefService.setBoolean(Pref.SIGNIN_ALLOWED, false);
                });

        // When user is signed out, a signed-out avatar should be visible on the NTP.
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        withContentDescription(
                                R.string.accessibility_toolbar_btn_signed_out_identity_disc)));

        // Clicking the signed-out avatar should lead to the settings screen.
        ActivityTestUtils.waitForActivity(
                InstrumentationRegistry.getInstrumentation(),
                SettingsActivity.class,
                () -> onView(withId(R.id.optional_toolbar_button)).perform(click()));
    }

    @Test
    @MediumTest
    // Specifies the test to run only with the GMS Core version greater than or equal to 22w30 which
    // is the min version that supports the local UPM backend, to avoid
    // SyncError.UPM_BACKEND_OUTDATED.
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    public void testIdentityDiscSignedIn() {
        // Identity Disc should be shown on sign-in state change with a NTP refresh.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        String expectedContentDescription =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string
                                        .accessibility_toolbar_btn_identity_disc_with_name_and_email,
                                TestAccounts.ACCOUNT1.getFullName(),
                                TestAccounts.ACCOUNT1.getEmail());
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        withContentDescription(expectedContentDescription)));

        mSigninTestRule.signOut();
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        withContentDescription(
                                R.string.accessibility_toolbar_btn_signed_out_identity_disc)));
    }

    @Test
    @MediumTest
    // Specifies the test to run only with the GMS Core version greater than or equal to 22w30 which
    // is the min version that supports the local UPM backend, to avoid
    // SyncError.UPM_BACKEND_OUTDATED.
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    public void testIdentityDiscSignedIn_nonDisplayableEmail() {
        // Identity Disc should be shown on sign-in state change with a NTP refresh.
        AccountInfo accountInfo = addAndSigninAccountWithNonDisplayableEmail();
        String expectedContentDescription =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string.accessibility_toolbar_btn_identity_disc_with_name,
                                accountInfo.getFullName());
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        withContentDescription(expectedContentDescription)));

        mSigninTestRule.forceSignOut();
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        withContentDescription(
                                R.string.accessibility_toolbar_btn_signed_out_identity_disc)));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP)
    public void testIdentityDiscWithErrorBadgeSignedIn() {
        // Fake an identity error.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FakeSyncServiceImpl fakeSyncServiceImpl = new FakeSyncServiceImpl();
                    SyncServiceFactory.setInstanceForTesting(fakeSyncServiceImpl);
                    fakeSyncServiceImpl.setRequiresClientUpgrade(true);
                });

        // Identity Disc should be shown on sign-in state change with a NTP refresh.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        String expectedContentDescription =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string
                                        .accessibility_toolbar_btn_identity_disc_error_with_name_and_email,
                                TestAccounts.ACCOUNT1.getFullName(),
                                TestAccounts.ACCOUNT1.getEmail());
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        withContentDescription(expectedContentDescription)));

        mSigninTestRule.signOut();
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        withContentDescription(
                                R.string.accessibility_toolbar_btn_signed_out_identity_disc)));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP)
    public void testIdentityDiscWithErrorBadgeSignedIn_nonDisplayableEmail() {
        // Fake an identity error.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FakeSyncServiceImpl fakeSyncServiceImpl = new FakeSyncServiceImpl();
                    SyncServiceFactory.setInstanceForTesting(fakeSyncServiceImpl);
                    fakeSyncServiceImpl.setRequiresClientUpgrade(true);
                });

        // Identity Disc should be shown on sign-in state change with a NTP refresh.
        AccountInfo accountInfo = addAndSigninAccountWithNonDisplayableEmail();
        String expectedContentDescription =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string.accessibility_toolbar_btn_identity_disc_error_with_name,
                                accountInfo.getFullName());
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        withContentDescription(expectedContentDescription)));

        mSigninTestRule.forceSignOut();
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        withContentDescription(
                                R.string.accessibility_toolbar_btn_signed_out_identity_disc)));
    }

    @Test
    @MediumTest
    @SuppressWarnings("CheckReturnValue")
    public void testIdentityDiscWithSwitchToIncognito() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        ViewUtils.waitForVisibleView(withId(R.id.optional_toolbar_button));

        // Identity Disc should not be visible, when switched from sign in state to incognito NTP.
        mActivityTestRule.newIncognitoTabFromMenu();
        ViewUtils.waitForViewCheckingState(
                withId(R.id.optional_toolbar_button), ViewUtils.VIEW_GONE);
    }

    @Test
    @SmallTest
    public void onPrimaryAccountChanged_accountSet() {
        IdentityDiscController identityDiscController =
                buildControllerWithObserver(mButtonDataObserver);
        PrimaryAccountChangeEvent accountSetEvent =
                newSigninEvent(PrimaryAccountChangeEvent.Type.SET);

        identityDiscController.onPrimaryAccountChanged(accountSetEvent);

        verify(mButtonDataObserver).buttonDataChanged(true);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP)
    public void preExistingErrorAtCreation() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Fake an identity error.
                    FakeSyncServiceImpl fakeSyncService = new FakeSyncServiceImpl();
                    SyncServiceFactory.setInstanceForTesting(fakeSyncService);
                    fakeSyncService.setRequiresClientUpgrade(true);

                    mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

                    ObservableSupplierImpl<Profile> profileSupplier =
                            new ObservableSupplierImpl<Profile>();
                    IdentityDiscController identityDiscController =
                            new IdentityDiscController(
                                    mActivityTestRule.getActivity(), mDispatcher, profileSupplier);

                    Assert.assertEquals(
                            SyncError.NO_ERROR, identityDiscController.getIdentityError());

                    identityDiscController.onFinishNativeInitialization();
                    profileSupplier.set(ProfileManager.getLastUsedRegularProfile());

                    Assert.assertEquals(
                            SyncError.CLIENT_OUT_OF_DATE,
                            identityDiscController.getIdentityError());
                });
    }

    @Test
    @SmallTest
    public void onPrimaryAccountChanged_accountCleared() {
        IdentityDiscController identityDiscController =
                buildControllerWithObserver(mButtonDataObserver);
        PrimaryAccountChangeEvent accountClearedEvent =
                newSigninEvent(PrimaryAccountChangeEvent.Type.CLEARED);
        identityDiscController.onPrimaryAccountChanged(accountClearedEvent);

        verify(mButtonDataObserver).buttonDataChanged(false);
        Assert.assertTrue(identityDiscController.isProfileDataCacheEmpty());
    }

    @Test
    @MediumTest
    public void onClick_profileSupplierNotYetInitialized_doesNothing() {
        TrackerFactory.setTrackerForTests(mTracker);
        IdentityDiscController identityDiscController =
                new IdentityDiscController(
                        mActivityTestRule.getActivity(), mDispatcher, /* profileSupplier= */ null);

        // If the button is tapped before the profile is set, the click shouldn't be recorded.
        identityDiscController.onClick();
        verifyNoMoreInteractions(mTracker);
    }

    @Test
    @MediumTest
    public void onClick_profileNotYetInitialized_doesNothing() {
        TrackerFactory.setTrackerForTests(mTracker);
        IdentityDiscController identityDiscController =
                new IdentityDiscController(
                        mActivityTestRule.getActivity(), mDispatcher, mProfileSupplier);

        // If the button is tapped before the profile is set, the click shouldn't be recorded.
        identityDiscController.onClick();
        verifyNoMoreInteractions(mTracker);
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testIdentityDisc_signedOut(boolean nightModeEnabled) throws IOException {
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(R.id.optional_toolbar_button),
                "identity_disc_signed_out");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    // Specifies the test to run only with the GMS Core version greater than or equal to 22w30 which
    // is the min version that supports the local UPM backend, to avoid
    // SyncError.UPM_BACKEND_OUTDATED.
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    public void testIdentityDisc_signedIn(boolean nightModeEnabled) throws IOException {
        // Sign-in and wait for the user profile image to appear.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        String expectedContentDescription =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string
                                        .accessibility_toolbar_btn_identity_disc_with_name_and_email,
                                TestAccounts.ACCOUNT1.getFullName(),
                                TestAccounts.ACCOUNT1.getEmail());
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        withContentDescription(expectedContentDescription)));

        // Test the profile image shown in signed-in state to ensure the image is not tinted
        // accidentally.
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(R.id.optional_toolbar_button),
                "identity_disc_signed_in");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    @EnableFeatures(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP)
    // Specifies the test to run only with the GMS Core version greater than or equal to 22w30 which
    // is the min version that supports the local UPM backend, to avoid
    // SyncError.UPM_BACKEND_OUTDATED.
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    public void testIdentityDisc_signedIn_unoPhase2FollowUpEnabled_noIdentityError(
            boolean nightModeEnabled) throws IOException {
        // Sign-in and wait for the user profile image to appear.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        String expectedContentDescription =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string
                                        .accessibility_toolbar_btn_identity_disc_with_name_and_email,
                                TestAccounts.ACCOUNT1.getFullName(),
                                TestAccounts.ACCOUNT1.getEmail());
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        withContentDescription(expectedContentDescription)));

        // Test the profile image shown in signed-in state to ensure the image is not tinted
        // accidentally.
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(R.id.optional_toolbar_button),
                "identity_disc_signed_in_no_identity_error");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    @EnableFeatures(ChromeFeatureList.UNO_PHASE_2_FOLLOW_UP)
    public void testIdentityDisc_signedIn_unoPhase2FollowUpEnabled_identityErrorExist(
            boolean nightModeEnabled) throws IOException {
        // Fake an identity error.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    FakeSyncServiceImpl fakeSyncServiceImpl = new FakeSyncServiceImpl();
                    SyncServiceFactory.setInstanceForTesting(fakeSyncServiceImpl);
                    fakeSyncServiceImpl.setRequiresClientUpgrade(true);
                });

        // Sign-in and wait for the user profile image to appear.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        String expectedContentDescription =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string
                                        .accessibility_toolbar_btn_identity_disc_error_with_name_and_email,
                                TestAccounts.ACCOUNT1.getFullName(),
                                TestAccounts.ACCOUNT1.getEmail());
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.optional_toolbar_button),
                        isDisplayed(),
                        withContentDescription(expectedContentDescription)));

        // Test the profile image shown with an error badge in signed-in state when an identity
        // error exist.
        mRenderTestRule.render(
                mActivityTestRule.getActivity().findViewById(R.id.optional_toolbar_button),
                "identity_disc_signed_in_identity_error_exist");
    }

    private void leaveNtp() {
        mActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        ChromeTabUtils.waitForTabPageLoaded(mTab, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    private AccountInfo addAndSigninAccountWithNonDisplayableEmail() {
        mSigninTestRule.addAccount(TestAccounts.CHILD_ACCOUNT_NON_DISPLAYABLE_EMAIL);
        mSigninTestRule.waitForSignin(TestAccounts.CHILD_ACCOUNT_NON_DISPLAYABLE_EMAIL);
        return TestAccounts.CHILD_ACCOUNT_NON_DISPLAYABLE_EMAIL;
    }

    private IdentityDiscController buildControllerWithObserver(
            ButtonDataProvider.ButtonDataObserver observer) {
        IdentityDiscController controller =
                new IdentityDiscController(
                        mActivityTestRule.getActivity(), mDispatcher, mProfileSupplier);
        controller.addObserver(observer);

        return controller;
    }

    private PrimaryAccountChangeEvent newSigninEvent(int eventType) {
        return new PrimaryAccountChangeEvent(eventType, ConsentLevel.SIGNIN);
    }
}
