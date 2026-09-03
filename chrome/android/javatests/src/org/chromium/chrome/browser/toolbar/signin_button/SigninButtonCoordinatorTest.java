// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.signin_button;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
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
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
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
// TODO(b/521895796, b/555414915): Update Android tests with WebUI NTP enabled on AL.
@DisableFeatures({ChromeFeatureList.SETTINGS_IN_TAB, ChromeFeatureList.USE_WEB_UI_NTP_ANDROID})
public class SigninButtonCoordinatorTest {

    // Mock sign-in environment needs to be destroyed after ChromeTabbedActivity in case there are
    // observers registered in the AccountManagerFacade mock.
    @Rule(order = 0)
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule(order = 1)
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private FakeSyncServiceImpl mFakeSyncServiceImpl;

    private RegularNewTabPageStation mPage;

    private String mContentDescriptionWithNameAndEmail;

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
        startActivityOnNtp();

        // Button to sign-in should be visible on NTP.
        verifySignedOutButtonVisible();
    }

    @Test
    @MediumTest
    public void testSigninButton_DisabledSignin_ShowsAvatar() {
        startActivityOnNtp();

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
        startActivityOnNtp();

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
        startActivityOnNtp();

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
        startActivityOnNtp();

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
        startActivityOnNtp();

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
        // Injects the mock SyncService before the Activity launches as the toolbar instantiates
        // SigninButtonCoordinator and binds the SyncService immediately upon creation.
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mFakeSyncServiceImpl = new FakeSyncServiceImpl();
                    SyncServiceFactory.setInstanceForTesting(mFakeSyncServiceImpl);
                });
        startActivityOnNtp();

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
        // Injects the null SyncService before the Activity launches as the toolbar instantiates
        // SigninButtonCoordinator and binds the SyncService immediately upon creation.
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    SyncServiceFactory.setInstanceForTesting(null);
                });
        startActivityOnNtp();

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
        startActivityOnNtp();

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
        startActivityOnNtp();

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
        startActivityOnNtp();

        // Initially visible on NTP.
        ViewUtils.waitForVisibleView(withId(R.id.signin_button));

        // Should be hidden on navigation away from NTP.
        mPage.loadWebPageProgrammatically(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        onView(withId(R.id.signin_button)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.PROFILE_DISC_ON_ALL_PAGES)
    public void testSigninButton_InflatesAndShowsWhenNavigatingToNtp() {
        // Start on a non-NTP page so button should not be inflated.
        WebPageStation blankPage = mActivityTestRule.startOnBlankPage();
        onView(withId(R.id.signin_button)).check(doesNotExist());

        // Navigate to the NTP. This triggers updateButtonVisibility -> inflation.
        blankPage.loadPageProgrammatically(
                getOriginalNativeNtpUrl(), RegularNewTabPageStation.newBuilder());

        ViewUtils.waitForVisibleView(withId(R.id.signin_button));
    }

    @Test
    @MediumTest
    public void testSigninButtonHiddenOnIncognitoNtp() {
        startActivityOnNtp();

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
        startActivityOnNtp();

        ViewUtils.waitForVisibleView(withId(R.id.signin_button));

        // Clicking the sign-in button should lead to the sign-in bottom sheet.
        onView(withId(R.id.signin_button)).perform(click());
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.account_picker_header_title),
                        withText(R.string.signin_account_picker_bottom_sheet_title)));
    }

    @Test
    @MediumTest
    @DisableFeatures(SigninFeatures.ENABLE_SEAMLESS_SIGNIN)
    public void testClickSigninButton_SignedOut_SeamlessSigninDisabled() {
        startActivityOnNtp();

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
        startActivityOnNtp();

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
        startActivityOnNtp();

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
    @Restriction(DeviceFormFactor.TABLET_OR_DESKTOP)
    public void testClickSigninButton_ClearsUrlFocus() {
        startActivityOnNtp();

        // Initially visible on NTP.
        ViewUtils.waitForVisibleView(withId(R.id.signin_button));

        // Focus the URL bar.
        OmniboxTestUtils omniboxTestUtils = new OmniboxTestUtils(mActivityTestRule.getActivity());
        omniboxTestUtils.requestFocus();

        // Signin button should still be visible on tablet.
        ViewUtils.waitForVisibleView(withId(R.id.signin_button));

        // Click the sign-in button.
        onView(withId(R.id.signin_button)).perform(click());

        // The URL bar should lose focus.
        omniboxTestUtils.checkFocus(false);
    }

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.PHONE)
    public void testSigninButtonHiddenOnUrlFocus() {
        startActivityOnNtp();

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
        startActivityOnNtp();

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
        startActivityOnNtp();

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

    @Test
    @MediumTest
    @Restriction(DeviceFormFactor.DESKTOP)
    @EnableFeatures(SigninFeatures.SIGNIN_BUTTON_PROFILE_MENU)
    public void testClickSigninButton_DesktopOpensAccountMenu() {
        DeviceInfo.setIsDesktopForTesting(true);
        startActivityOnNtp();

        AppHeaderUtils.setAppInDesktopWindowForTesting(true);
        ViewUtils.waitForVisibleView(withId(R.id.signin_button));

        onView(withId(R.id.signin_button)).perform(click());

        // Verify that the account menu popup is displayed.
        ViewUtils.waitForVisibleView(withId(R.id.account_menu_container));
    }

    private void startActivityOnNtp() {
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

    private void verifySignedOutButtonVisible() {
        ViewUtils.waitForVisibleView(
                allOf(
                        withId(R.id.avatar_button),
                        isDisplayed(),
                        withContentDescription(
                                R.string.accessibility_toolbar_btn_signed_out_identity_disc)));
    }

    private void setSigninAllowed(boolean allowed) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.SIGNIN_ALLOWED, allowed);
                });
    }
}
