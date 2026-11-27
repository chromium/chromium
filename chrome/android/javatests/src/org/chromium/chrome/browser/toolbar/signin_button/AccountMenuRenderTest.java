// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.signin_button;

import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;

import static org.chromium.base.test.transit.ViewFinder.waitForView;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameterBefore;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.ImportantFormFactors;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.sync.FakeSyncServiceImpl;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.NightModeTestUtils.NightModeParams;
import org.chromium.ui.test.util.ViewUtils;

import java.io.IOException;

/** Render tests for the Account Menu popup on desktop Android. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@Batch(Batch.PER_CLASS)
@ImportantFormFactors(DeviceFormFactor.DESKTOP)
@EnableFeatures({
    SigninFeatures.SIGNIN_BUTTON_PROFILE_MENU,
    SigninFeatures.SIGNIN_BUTTON_PROFILE_MENU_REFINEMENTS,
})
public class AccountMenuRenderTest {

    // Mock sign-in environment needs to be destroyed after ChromeTabbedActivity in case there are
    // observers registered in the AccountManagerFacade mock.
    @Rule(order = 0)
    public final SigninTestRule mSigninTestRule = SigninTestRule.createWithCleanups();

    @Rule(order = 1)
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule(order = 2)
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.SERVICES_SIGN_IN)
                    .build();

    @UseMethodParameterBefore(NightModeParams.class)
    public void setupNightMode(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        DeviceInfo.setIsDesktopForTesting(true);
        mActivityTestRule.startOnNtp();
    }

    @After
    public void tearDown() {
        if (ProfileManager.isInitialized()) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                                .clearPref(Pref.SIGNIN_ALLOWED);
                    });
        }
        DeviceInfo.resetIsDesktopForTesting();
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @UseMethodParameter(NightModeParams.class)
    public void testAccountMenu_SignedOut(boolean nightModeEnabled) throws IOException {
        openAccountMenu();
        waitForView(withId(R.id.account_menu_signin_button));

        renderAccountMenu("account_menu_signed_out");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @UseMethodParameter(NightModeParams.class)
    public void testAccountMenu_SignedOut_SigninDisabled(boolean nightModeEnabled)
            throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UserPrefs.get(ProfileManager.getLastUsedRegularProfile())
                            .setBoolean(Pref.SIGNIN_ALLOWED, false);
                });

        openAccountMenu();
        waitForView(withText(R.string.menu_passwords_and_autofill));

        renderAccountMenu("account_menu_signed_out_signin_disabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @UseMethodParameter(NightModeParams.class)
    public void testAccountMenu_SignedIn(boolean nightModeEnabled) throws IOException {
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);

        openAccountMenu();
        // Wait for the identity card to be populated with the signed-in account's profile data.
        waitForView(
                allOf(
                        withId(R.id.account_menu_identity_name),
                        withText(TestAccounts.ACCOUNT1.getFullName())));
        waitForView(
                allOf(
                        withId(R.id.account_menu_identity_email),
                        withText(TestAccounts.ACCOUNT1.getEmail())));

        renderAccountMenu("account_menu_signed_in");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @UseMethodParameter(NightModeParams.class)
    public void testAccountMenu_SignedIn_ManagedAccount(boolean nightModeEnabled)
            throws IOException {
        mSigninTestRule.addAccountThenSignin(TestAccounts.MANAGED_ACCOUNT);

        openAccountMenu();
        waitForView(withId(R.id.account_menu_managed_header));
        waitForView(
                allOf(
                        withId(R.id.account_menu_identity_name),
                        withText(TestAccounts.MANAGED_ACCOUNT.getFullName())));

        renderAccountMenu("account_menu_signed_in_managed_account");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    @EnableFeatures(SigninFeatures.SIGNIN_BUTTON_PROFILE_MENU_ERROR_CARD)
    @UseMethodParameter(NightModeParams.class)
    public void testAccountMenu_SignedIn_WithErrorCard(boolean nightModeEnabled)
            throws IOException {
        FakeSyncServiceImpl fakeSyncService =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            FakeSyncServiceImpl syncService = new FakeSyncServiceImpl();
                            SyncServiceFactory.setInstanceForTesting(syncService);
                            return syncService;
                        });
        mSigninTestRule.addAccountThenSignin(TestAccounts.ACCOUNT1);
        fakeSyncService.setRequiresClientUpgrade(true);

        openAccountMenu();
        waitForView(withId(R.id.signin_settings_card));
        waitForView(
                allOf(
                        withId(R.id.account_menu_identity_name),
                        withText(TestAccounts.ACCOUNT1.getFullName())));

        renderAccountMenu("account_menu_signed_in_with_error_card");
    }

    private void openAccountMenu() {
        waitForView(withId(R.id.signin_button)).click();
        waitForView(withId(R.id.account_menu_container));
    }

    private void renderAccountMenu(String renderId) throws IOException {
        View menuView = waitForView(withId(R.id.account_menu_container)).getView();
        ViewUtils.waitForStableView(menuView);
        mRenderTestRule.render(menuView, renderId);
    }
}
