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

import android.view.View;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
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

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.signin.SyncConsentActivity;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ButtonDataProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.identitymanager.PrimaryAccountChangeEvent;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.test.util.ViewUtils;

/**
 * Instrumentation test for Identity Disc.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class IdentityDiscControllerTest {
    private static final String EMAIL = "email@gmail.com";
    private static final String NAME = "Email Emailson";
    private static final String FULL_NAME = NAME + ".full";
    private static final ObservableSupplier<Profile> EMPTY_PROFILE_SUPPLIER =
            new ObservableSupplier<>() {
                @Override
                public Profile addObserver(Callback<Profile> obs) {
                    return null;
                }

                @Override
                public void removeObserver(Callback<Profile> obs) {}

                @Override
                public Profile get() {
                    return null;
                }
            };

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

    private Tab mTab;

    @Mock
    private IdentityServicesProvider mIdentityServicesProviderMock;
    @Mock
    private SigninManager mSigninManagerMock;
    @Mock
    private IdentityManager mIdentityManagerMock;
    @Mock
    private ObservableSupplier<Profile> mProfileSupplier;
    @Mock
    private ButtonDataProvider.ButtonDataObserver mButtonDataObserver;
    @Mock
    private Tracker mTracker;
    @Mock
    private ActivityLifecycleDispatcher mDispatcher;

    @Before
    public void setUp() {
        mActivityTestRule.startMainActivityWithURL(UrlConstants.NTP_URL);
        mTab = mActivityTestRule.getActivity().getActivityTab();
        NewTabPageTestUtils.waitForNtpLoaded(mTab);
    }

    @Test
    @MediumTest
    public void testIdentityDiscWithNavigation() {
        // User is signed in.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        ViewUtils.waitForVisibleView(allOf(withId(R.id.optional_toolbar_button), isDisplayed()));

        // Identity Disc should be hidden on navigation away from NTP.
        leaveNTP();
        onView(withId(R.id.optional_toolbar_button))
                .check(matches(anyOf(withEffectiveVisibility(ViewMatchers.Visibility.GONE),
                        not(withContentDescription(
                                R.string.accessibility_toolbar_btn_identity_disc)))));
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.IDENTITY_STATUS_CONSISTENCY})
    @SuppressWarnings("CheckReturnValue")
    public void testIdentityDiscWithSignin() {
        // When user is signed out and IdentityStatusConsistency is disabled, Identity Disc should
        // not be visible on the NTP.
        onView(withId(R.id.optional_toolbar_button)).check((view, noViewException) -> {
            if (view != null) {
                ViewMatchers.assertThat("IdentityDisc view should be gone if it exists",
                        view.getVisibility(), Matchers.is(View.GONE));
            }
        });

        // Identity Disc should be shown on sign-in state change with a NTP refresh.
        mSigninTestRule.addTestAccountThenSignin();
        // TODO(https://crbug.com/1132291): Remove the reload once the sign-in without sync observer
        //  is implemented.
        TestThreadUtils.runOnUiThreadBlocking(mTab::reload);
        // TODO(crbug.com/1469988): This is a no-op, replace with ViewUtils.waitForVisibleView().
        ViewUtils.isEventuallyVisible(allOf(withId(R.id.optional_toolbar_button), isDisplayed(),
                withContentDescription(R.string.accessibility_toolbar_btn_identity_disc)));

        mSigninTestRule.signOut();
        // TODO(crbug.com/1469988): This is a no-op, replace with ViewUtils.waitForVisibleView().
        ViewUtils.isEventuallyVisible(allOf(withId(R.id.optional_toolbar_button),
                withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.IDENTITY_STATUS_CONSISTENCY})
    public void testIdentityDiscSignedOut_identityStatusConsistencyEnabled() {
        // When user is signed out, a signed-out avatar should be visible on the NTP.
        ViewUtils.waitForVisibleView(allOf(withId(R.id.optional_toolbar_button), isDisplayed(),
                withContentDescription(
                        R.string.accessibility_toolbar_btn_signed_out_identity_disc)));

        // Clicking the signed-out avatar should lead to the sync consent screen.
        ActivityTestUtils.waitForActivity(InstrumentationRegistry.getInstrumentation(),
                SyncConsentActivity.class,
                () -> onView(withId(R.id.optional_toolbar_button)).perform(click()));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.IDENTITY_STATUS_CONSISTENCY})
    public void
    testIdentityDiscSignedOut_signinDisabledByPolicy_identityStatusConsistencyEnabled() {
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            when(mIdentityServicesProviderMock.getSigninManager(
                         Profile.getLastUsedRegularProfile()))
                    .thenReturn(mSigninManagerMock);
            // This mock is required because the MainSettings class calls the IdentityManager.
            when(mIdentityServicesProviderMock.getIdentityManager(
                         Profile.getLastUsedRegularProfile()))
                    .thenReturn(mIdentityManagerMock);
        });
        when(mSigninManagerMock.isSigninDisabledByPolicy()).thenReturn(true);

        // When user is signed out, a signed-out avatar should be visible on the NTP.
        ViewUtils.waitForVisibleView(allOf(withId(R.id.optional_toolbar_button), isDisplayed(),
                withContentDescription(
                        R.string.accessibility_toolbar_btn_signed_out_identity_disc)));

        // Clicking the signed-out avatar should lead to the settings screen.
        ActivityTestUtils.waitForActivity(InstrumentationRegistry.getInstrumentation(),
                SettingsActivity.class,
                () -> onView(withId(R.id.optional_toolbar_button)).perform(click()));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.IDENTITY_STATUS_CONSISTENCY})
    public void testIdentityDiscWithSignin_identityStatusConsistencyEnabled() {
        // Identity Disc should be shown on sign-in state change with a NTP refresh.
        mSigninTestRule.addAccountThenSignin(EMAIL, NAME);
        // TODO(https://crbug.com/1132291): Remove the reload once the sign-in without sync observer
        //  is implemented.
        TestThreadUtils.runOnUiThreadBlocking(mTab::reload);
        String expectedContentDescription = mActivityTestRule.getActivity().getString(
                R.string.accessibility_toolbar_btn_identity_disc_with_name_and_email, FULL_NAME,
                EMAIL);
        ViewUtils.waitForVisibleView(allOf(withId(R.id.optional_toolbar_button), isDisplayed(),
                withContentDescription(expectedContentDescription)));
        mSigninTestRule.signOut();
        ViewUtils.waitForVisibleView(allOf(withId(R.id.optional_toolbar_button), isDisplayed(),
                withContentDescription(
                        R.string.accessibility_toolbar_btn_signed_out_identity_disc)));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.IDENTITY_STATUS_CONSISTENCY})
    public void testIdentityDiscWithSignin_nonDisplayableEmail_identityStatusConsistencyEnabled() {
        // Identity Disc should be shown on sign-in state change with a NTP refresh.
        CoreAccountInfo coreAccountInfo = addAccountWithNonDisplayableEmail(NAME);
        SigninTestUtil.signin(coreAccountInfo);
        // TODO(https://crbug.com/1132291): Remove the reload once the sign-in without sync observer
        //  is implemented.
        TestThreadUtils.runOnUiThreadBlocking(mTab::reload);
        String expectedContentDescription = mActivityTestRule.getActivity().getString(
                R.string.accessibility_toolbar_btn_identity_disc_with_name, FULL_NAME);
        ViewUtils.waitForVisibleView(allOf(withId(R.id.optional_toolbar_button), isDisplayed(),
                withContentDescription(expectedContentDescription)));

        mSigninTestRule.signOut();
        ViewUtils.waitForVisibleView(allOf(withId(R.id.optional_toolbar_button), isDisplayed(),
                withContentDescription(
                        R.string.accessibility_toolbar_btn_signed_out_identity_disc)));
    }

    @Test
    @MediumTest
    @DisableFeatures({ChromeFeatureList.IDENTITY_STATUS_CONSISTENCY})
    @SuppressWarnings("CheckReturnValue")
    public void testIdentityDiscWithSigninAndEnableSync() {
        // When user is signed out and IdentityStatusConsistency is disabled, Identity Disc should
        // not be visible on the NTP.
        onView(withId(R.id.optional_toolbar_button)).check((view, noViewException) -> {
            if (view != null) {
                ViewMatchers.assertThat("IdentityDisc view should be gone if it exists",
                        view.getVisibility(), Matchers.is(View.GONE));
            }
        });

        // Identity Disc should be shown on sign-in state change without NTP refresh.
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        // TODO(crbug.com/1469988): This is a no-op, replace with ViewUtils.waitForVisibleView().
        ViewUtils.isEventuallyVisible(allOf(withId(R.id.optional_toolbar_button),
                withContentDescription(R.string.accessibility_toolbar_btn_identity_disc),
                isDisplayed()));

        mSigninTestRule.signOut();
        // TODO(crbug.com/1469988): This is a no-op, replace with ViewUtils.waitForVisibleView().
        ViewUtils.isEventuallyVisible(allOf(withId(R.id.optional_toolbar_button),
                withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.IDENTITY_STATUS_CONSISTENCY})
    @SuppressWarnings("CheckReturnValue")
    public void testIdentityDiscWithSigninAndEnableSync_identityStatusConsistencyEnabled() {
        // Identity Disc should be shown on sign-in state change without NTP refresh.
        mSigninTestRule.addAccountThenSigninAndEnableSync(EMAIL, NAME);
        String expectedContentDescription = mActivityTestRule.getActivity().getString(
                R.string.accessibility_toolbar_btn_identity_disc_with_name_and_email, FULL_NAME,
                EMAIL);
        // TODO(crbug.com/1469988): This is a no-op, replace with ViewUtils.waitForVisibleView().
        ViewUtils.isEventuallyVisible(allOf(withId(R.id.optional_toolbar_button),
                withContentDescription(expectedContentDescription), isDisplayed()));

        mSigninTestRule.signOut();
        // TODO(crbug.com/1469988): This is a no-op, replace with ViewUtils.waitForVisibleView().
        ViewUtils.isEventuallyVisible(allOf(withId(R.id.optional_toolbar_button),
                withContentDescription(R.string.accessibility_toolbar_btn_signed_out_identity_disc),
                isDisplayed()));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.IDENTITY_STATUS_CONSISTENCY})
    public void
    testIdentityDiscWithSigninAndEnableSync_nonDisplayableEmail_identityStatusConsistencyEnabled() {
        // Identity Disc should be shown on sign-in state change without NTP refresh.
        CoreAccountInfo coreAccountInfo = addAccountWithNonDisplayableEmail(NAME);
        SigninTestUtil.signinAndEnableSync(coreAccountInfo,
                TestThreadUtils.runOnUiThreadBlockingNoException(SyncServiceFactory::get));
        String expectedContentDescription = mActivityTestRule.getActivity().getString(
                R.string.accessibility_toolbar_btn_identity_disc_with_name, FULL_NAME);
        ViewUtils.waitForVisibleView(allOf(withId(R.id.optional_toolbar_button),
                withContentDescription(expectedContentDescription), isDisplayed()));

        mSigninTestRule.signOut();
        ViewUtils.waitForVisibleView(allOf(withId(R.id.optional_toolbar_button),
                withContentDescription(R.string.accessibility_toolbar_btn_signed_out_identity_disc),
                isDisplayed()));
    }

    @Test
    @MediumTest
    @SuppressWarnings("CheckReturnValue")
    public void testIdentityDiscWithSwitchToIncognito() {
        mSigninTestRule.addTestAccountThenSigninAndEnableSync();
        // TODO(crbug.com/1469988): This is a no-op, replace with ViewUtils.waitForVisibleView().
        ViewUtils.isEventuallyVisible(allOf(withId(R.id.optional_toolbar_button), isDisplayed()));

        // Identity Disc should not be visible, when switched from sign in state to incognito NTP.
        mActivityTestRule.newIncognitoTabFromMenu();
        // TODO(crbug.com/1469988): This is a no-op, replace with ViewUtils.waitForVisibleView().
        ViewUtils.isEventuallyVisible(allOf(withId(R.id.optional_toolbar_button),
                withEffectiveVisibility(ViewMatchers.Visibility.GONE)));
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
        IdentityDiscController identityDiscController = new IdentityDiscController(
                mActivityTestRule.getActivity(), mDispatcher, /*profileSupplier=*/null);

        // If the button is tapped before the profile is set, the click shouldn't be recorded.
        identityDiscController.onClick();
        verifyNoMoreInteractions(mTracker);
    }

    @Test
    @MediumTest
    public void onClick_profileNotYetInitialized_doesNothing() {
        TrackerFactory.setTrackerForTests(mTracker);
        IdentityDiscController identityDiscController = new IdentityDiscController(
                mActivityTestRule.getActivity(), mDispatcher, EMPTY_PROFILE_SUPPLIER);

        // If the button is tapped before the profile is set, the click shouldn't be recorded.
        identityDiscController.onClick();
        verifyNoMoreInteractions(mTracker);
    }

    private void leaveNTP() {
        mActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        ChromeTabUtils.waitForTabPageLoaded(mTab, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
    }

    private CoreAccountInfo addAccountWithNonDisplayableEmail(String name) {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccount(
                EMAIL, name, SigninTestRule.NON_DISPLAYABLE_EMAIL_ACCOUNT_CAPABILITIES);
        mSigninTestRule.waitForSeeding();
        return coreAccountInfo;
    }

    private IdentityDiscController buildControllerWithObserver(
            ButtonDataProvider.ButtonDataObserver observer) {
        IdentityDiscController controller = new IdentityDiscController(
                mActivityTestRule.getActivity(), mDispatcher, EMPTY_PROFILE_SUPPLIER);
        controller.addObserver(observer);

        return controller;
    }

    private PrimaryAccountChangeEvent newSigninEvent(int eventType) {
        return new PrimaryAccountChangeEvent(eventType, ConsentLevel.SIGNIN);
    }
}
