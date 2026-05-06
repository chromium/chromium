// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.signin_button;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anyOf;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;

import static org.chromium.chrome.browser.url_constants.UrlConstantResolver.getOriginalNativeNtpUrl;

import android.app.Activity;
import android.content.res.ColorStateList;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivity;
import org.chromium.chrome.browser.sync.FakeSyncServiceImpl;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.OmniboxTestUtils;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;
import org.chromium.ui.test.util.ViewUtils;
import org.chromium.ui.widget.ChromeImageButton;

/** Integration tests for {@link SigninButtonCoordinator}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@DoNotBatch(reason = "This test relies on native initialization")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({SigninFeatures.SIGNIN_LEVEL_UP_BUTTON, SigninFeatures.PROFILE_DISC_ON_ALL_PAGES})
public class SigninButtonCoordinatorTest {

    @Rule(order = 1)
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    // Mock sign-in environment needs to be destroyed after ChromeTabbedActivity in case there are
    // observers registered in the AccountManagerFacade mock.
    @Rule(order = 0)
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    private FakeSyncServiceImpl mFakeSyncServiceImpl;

    private RegularNewTabPageStation mPage;

    private String mContentDescriptionWithNameAndEmail;

    @Before
    public void setUp() {
        mPage = mActivityTestRule.startOnNtp();
        NewTabPageTestUtils.waitForNtpLoaded(mPage.getTab());
        mContentDescriptionWithNameAndEmail =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string
                                        .accessibility_toolbar_btn_identity_disc_with_name_and_email,
                                TestAccounts.ACCOUNT1.getFullName(),
                                TestAccounts.ACCOUNT1.getEmail());
    }

    @After
    public void tearDown() {
        if (mFakeSyncServiceImpl != null) {
            mFakeSyncServiceImpl = null;
            SyncServiceFactory.setInstanceForTesting(null);
        }
        setSigninAllowed(true);
    }

    @Test
    @MediumTest
    public void testSigninButtonVisibleOnNtp() {
        // Button to sign-in should be visible on NTP.
        verifySignedOutButtonVisible();
    }

    @Test
    @MediumTest
    public void testSigninButton_DisabledSignin_ShowsAvatar() {
        setSigninAllowed(false);

        // Should show signed-out avatar instead of text button.
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.avatar_button),
                        isDisplayed(),
                        withContentDescription(
                                R.string.accessibility_toolbar_btn_signed_out_identity_disc)));

        setSigninAllowed(true);

        // Should show sign-in button.
        verifySignedOutButtonVisible();
    }

    @Test
    @MediumTest
    // Specifies the test to run only with the GMS Core version greater than or equal to 24w15 which
    // is the min version that supports split stores UPM backend, to avoid
    // UserActionableError.NEEDS_UPM_BACKEND_UPGRADE.
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    public void testSignIn_ShowsPersonalizedIdentityDisc() {
        // Initially shows sign-in button.
        verifySignedOutButtonVisible();

        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        // Avatar should update to a personalized disc with a name and email in its description.
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.avatar_button),
                        isDisplayed(),
                        withContentDescription(mContentDescriptionWithNameAndEmail)));
    }

    @Test
    @MediumTest
    // Specifies the test to run only with the GMS Core version greater than or equal to 24w15 which
    // is the min version that supports split stores UPM backend, to avoid
    // UserActionableError.NEEDS_UPM_BACKEND_UPGRADE.
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    public void testSignIn_ShowsPersonalizedIdentityDiscNonDisplayableEmail() {
        // Initially shows sign-in button.
        verifySignedOutButtonVisible();

        mSigninTestRule.addAccount(TestAccounts.CHILD_ACCOUNT_NON_DISPLAYABLE_EMAIL);
        mSigninTestRule.waitForSignin(TestAccounts.CHILD_ACCOUNT_NON_DISPLAYABLE_EMAIL);

        // Avatar should update to a personalized disc with a name in its description.
        String expectedDescription =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string.accessibility_toolbar_btn_identity_disc_with_name,
                                TestAccounts.CHILD_ACCOUNT_NON_DISPLAYABLE_EMAIL.getFullName());
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.avatar_button),
                        isDisplayed(),
                        withContentDescription(expectedDescription)));
    }

    @Test
    @MediumTest
    // Specifies the test to run only with the GMS Core version greater than or equal to 24w15 which
    // is the min version that supports split stores UPM backend, to avoid
    // UserActionableError.NEEDS_UPM_BACKEND_UPGRADE.
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    public void testSignIn_ShowsPersonalizedIdentityDiscNoName() {
        // Initially shows sign-in button.
        verifySignedOutButtonVisible();

        mSigninTestRule.addAccount(TestAccounts.CHILD_ACCOUNT_NON_DISPLAYABLE_EMAIL_AND_NO_NAME);
        mSigninTestRule.waitForSignin(TestAccounts.CHILD_ACCOUNT_NON_DISPLAYABLE_EMAIL_AND_NO_NAME);

        // Avatar should update to a personalized disc with the fallback name in its description.
        String expectedDescription =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string.accessibility_toolbar_btn_identity_disc_with_name,
                                mActivityTestRule
                                        .getActivity()
                                        .getString(R.string.default_google_account_username));
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.avatar_button),
                        isDisplayed(),
                        withContentDescription(expectedDescription)));
    }

    @Test
    @MediumTest
    // Specifies the test to run only with the GMS Core version greater than or equal to 24w15 which
    // is the min version that supports split stores UPM backend, to avoid
    // UserActionableError.NEEDS_UPM_BACKEND_UPGRADE.
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    public void testSignOut_ShowsSigninTextButton() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        // Initially shows the user's avatar with a personalized description.
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.avatar_button),
                        isDisplayed(),
                        withContentDescription(mContentDescriptionWithNameAndEmail)));

        mSigninTestRule.signOut();

        // Should update to the sign-in button.
        verifySignedOutButtonVisible();
    }

    @Test
    @MediumTest
    // Specifies the test to run only with the GMS Core version greater than or equal to 24w15 which
    // is the min version that supports split stores UPM backend, to avoid
    // UserActionableError.NEEDS_UPM_BACKEND_UPGRADE.
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    public void testSigninButtonWithErrorBadge() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mFakeSyncServiceImpl = new FakeSyncServiceImpl();
                    SyncServiceFactory.setInstanceForTesting(mFakeSyncServiceImpl);
                });

        // SigninButton may have already been initialized with a real SyncService. As such,
        // recreating the activity in order to ensure the fake SyncService override is used.
        mActivityTestRule.recreateActivity();

        // Test initial state with no error.
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.avatar_button),
                        isDisplayed(),
                        withContentDescription(mContentDescriptionWithNameAndEmail)));

        // Test transition to error.
        mFakeSyncServiceImpl.setRequiresClientUpgrade(true);

        String expectedErrorContentDescription =
                mActivityTestRule
                        .getActivity()
                        .getString(
                                R.string
                                        .accessibility_toolbar_btn_identity_disc_error_with_name_and_email,
                                TestAccounts.ACCOUNT1.getFullName(),
                                TestAccounts.ACCOUNT1.getEmail());
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.avatar_button),
                        isDisplayed(),
                        withContentDescription(expectedErrorContentDescription)));

        // Test transition to signed in state with error resolved.
        mFakeSyncServiceImpl.setRequiresClientUpgrade(false);
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.avatar_button),
                        isDisplayed(),
                        withContentDescription(mContentDescriptionWithNameAndEmail)));
    }

    @Test
    @MediumTest
    // Specifies the test to run only with the GMS Core version greater than or equal to 24w15 which
    // is the min version that supports split stores UPM backend, to avoid
    // UserActionableError.NEEDS_UPM_BACKEND_UPGRADE.
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    public void testSigninButtonWithNullSyncService() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SyncServiceFactory.setInstanceForTesting(null);
                });

        // SigninButton may have already been initialized with a real SyncService. As such,
        // recreating the activity in order to ensure the null SyncService override is used.
        mActivityTestRule.recreateActivity();

        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        // Avatar should update to a personalized disc with a name and email in its description.
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.avatar_button),
                        isDisplayed(),
                        withContentDescription(mContentDescriptionWithNameAndEmail)));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testSigninButtonHiddenOnNavigationOnPhone() {
        // Initially visible on NTP.
        ViewUtils.waitForVisibleView(withId(R.id.signin_button));

        // Should be hidden on navigation away from NTP.
        WebPageStation aboutBlank =
                mPage.loadWebPageProgrammatically(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        onView(withId(R.id.signin_button)).check(matches(not(isDisplayed())));

        // Should be visible again when navigating back to NTP.
        aboutBlank.loadPageProgrammatically(
                getOriginalNativeNtpUrl(), RegularNewTabPageStation.newBuilder());
        ViewUtils.waitForVisibleView(withId(R.id.signin_button));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testSigninButtonShownOnNavigationOnTablet() {
        // Initially visible on NTP.
        ViewUtils.waitForVisibleView(withId(R.id.signin_button));

        // Should still be visible on navigation away from NTP.
        mPage.loadWebPageProgrammatically(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        ViewUtils.waitForVisibleView(withId(R.id.signin_button));
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.PROFILE_DISC_ON_ALL_PAGES)
    public void testSigninButtonHiddenOnNavigation() {
        // Initially visible on NTP.
        ViewUtils.waitForVisibleView(withId(R.id.signin_button));

        // Should be hidden on navigation away from NTP.
        mPage.loadWebPageProgrammatically(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        onView(withId(R.id.signin_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    // TODO(crbug.com/496912352): Not including Brya as inconsistent test state causes flakiness.
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void testSigninButtonHiddenOnIncognitoNtp() {
        // Initially visible on NTP.
        ViewUtils.waitForVisibleView(withId(R.id.signin_button));

        mPage.openNewIncognitoTabOrWindowFast();

        // Signin button should not be visible on incognito NTP.
        // It may not be inflated yet in the new incognito tab, so we check for both the
        // inflated view and its stub.
        onView(anyOf(withId(R.id.signin_button), withId(R.id.signin_button_stub)))
                .check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testClickSigninButton_SignedOut() {
        ViewUtils.waitForVisibleView(withId(R.id.signin_button));

        // Clicking the sign-in button should lead to the sign-in bottom sheet.
        onView(withId(R.id.signin_button)).perform(click());
        ViewUtils.waitForVisibleView(withText(R.string.signin_account_picker_bottom_sheet_title));
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testClickSigninButton_SignedOut_SeamlessSigninDisabled() {
        ViewUtils.waitForVisibleView(withId(R.id.signin_button));

        // Clicking the signed-out button should lead to the sign-in activity.
        Activity signinActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SigninAndHistorySyncActivity.class,
                        () -> onView(withId(R.id.signin_button)).perform(click()));
        assertNotNull("Signin activity should not be null.", signinActivity);
        ViewUtils.waitForVisibleView(withText(R.string.signin_account_picker_bottom_sheet_title));
        ApplicationTestUtils.finishActivity(signinActivity);
    }

    @Test
    @MediumTest
    public void testClickSigninButton_SignedOut_SigninDisabled() {
        setSigninAllowed(false);
        ViewUtils.waitForVisibleView(withId(R.id.signin_button));

        // Clicking the avatar should lead to the settings screen when signin is disabled.
        Activity settingsActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SettingsActivity.class,
                        () -> onView(withId(R.id.signin_button)).perform(click()));
        ApplicationTestUtils.finishActivity(settingsActivity);
    }

    @Test
    @MediumTest
    // Specifies the test to run only with the GMS Core version greater than or equal to 24w15 which
    // is the min version that supports split stores UPM backend, to avoid
    // UserActionableError.NEEDS_UPM_BACKEND_UPGRADE.
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    public void testClickSigninButton_SignedIn() {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.avatar_button),
                        isDisplayed(),
                        withContentDescription(mContentDescriptionWithNameAndEmail)));

        // Clicking the signed-in avatar should lead to the settings screen.
        Activity settingsActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        SettingsActivity.class,
                        () -> onView(withId(R.id.signin_button)).perform(click()));
        ApplicationTestUtils.finishActivity(settingsActivity);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testSigninButtonHiddenOnUrlFocus() {
        // Initially visible on NTP.
        ViewUtils.waitForVisibleView(withId(R.id.signin_button));

        // Focus the URL bar.
        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(mActivityTestRule.getActivity());
        omniboxTestUtils.requestFocus();

        // Signin button should be hidden when URL bar is focused.
        onView(withId(R.id.signin_button)).check(matches(not(isDisplayed())));

        // Clear focus from the URL bar.
        omniboxTestUtils.clearFocus();

        // Signin button should be visible again.
        ViewUtils.waitForVisibleView(withId(R.id.signin_button));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    public void testSigninButtonDisabledOnInactiveWindow() {
        AppHeaderUtils.setAppInDesktopWindowForTesting(true);
        ViewUtils.waitForVisibleView(withId(R.id.avatar_button));
        onView(withId(R.id.avatar_button)).check(matches(isEnabled()));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getActivity().onTopResumedActivityChanged(false);
                });

        onView(withId(R.id.avatar_button)).check(matches(not(isEnabled())));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule.getActivity().onTopResumedActivityChanged(true);
                });

        onView(withId(R.id.avatar_button)).check(matches(isEnabled()));
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP_FREEFORM)
    public void testSigninButtonAvatarTintChangesOnInactiveWindow() {
        AppHeaderUtils.setAppInDesktopWindowForTesting(true);
        setSigninAllowed(false);
        ViewUtils.waitForVisibleView(withId(R.id.avatar_button));

        ChromeImageButton avatarButton =
                mActivityTestRule.getActivity().findViewById(R.id.avatar_button);
        ColorStateList focusedTint = avatarButton.getImageTintList();
        assertNotNull(focusedTint);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Trigger window focus change to false.
                    mActivityTestRule.getActivity().onTopResumedActivityChanged(false);
                });
        ColorStateList unfocusedTint = avatarButton.getImageTintList();
        assertNotNull(unfocusedTint);
        assertNotEquals("Tint should change when window is inactive", focusedTint, unfocusedTint);
    }

    private void verifySignedOutButtonVisible() {
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivityTestRule.getActivity())) {
            ViewUtils.waitForVisibleView(
                    allOf(
                            withId(R.id.avatar_button),
                            isDisplayed(),
                            withContentDescription(
                                    R.string.accessibility_toolbar_btn_signed_out_identity_disc)));
        } else {
            ViewUtils.waitForVisibleView(
                    allOf(
                            withId(R.id.signin_text_button),
                            isDisplayed(),
                            withText(R.string.signin_promo_sign_in)));
        }
    }

    private void setSigninAllowed(boolean allowed) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.SIGNIN_ALLOWED, allowed);
                });
    }
}
