// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;
import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.PreferenceMatchers.withKey;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.os.Build;
import android.support.test.InstrumentationRegistry;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.about_settings.AboutChromeSettings;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillProfilesFragment;
import org.chromium.chrome.browser.download.settings.DownloadSettings;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.language.settings.LanguageSettings;
import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_manager.settings.PasswordSettings;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineSettings;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.sync.settings.AccountManagementFragment;
import org.chromium.chrome.browser.sync.settings.SyncPromoPreference;
import org.chromium.chrome.browser.sync.settings.SyncPromoPreference.State;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SyncPromoController;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.settings.SettingsFeatureList;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.io.IOException;
import java.util.HashSet;

/**
 * Test for {@link MainSettings}. Main purpose is to have a sanity check on the xml.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "show-autofill-signatures"})
@DisableFeatures(ChromeFeatureList.TANGIBLE_SYNC)
@DoNotBatch(reason = "Tests cannot run batched because they launch a Settings activity.")
public class MainSettingsFragmentTest {
    private static final String SEARCH_ENGINE_SHORT_NAME = "Google";

    private static final int RENDER_TEST_REVISION = 8;

    private final HomepageTestRule mHomepageTestRule = new HomepageTestRule();

    private final SyncTestRule mSyncTestRule = new SyncTestRule();

    private final SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(MainSettings.class);

    // SettingsActivity needs to be initialized and destroyed with the mock
    // signin environment setup in SyncTestRule
    @Rule
    public final RuleChain mRuleChain = RuleChain.outerRule(mSyncTestRule)
                                                .around(mHomepageTestRule)
                                                .around(mSettingsActivityTestRule);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_SETTINGS)
                    .build();
    @Mock
    public TemplateUrlService mMockTemplateUrlService;
    @Mock
    public TemplateUrl mMockSearchEngine;

    @Mock
    private PasswordCheck mPasswordCheck;

    @Mock
    private SyncConsentActivityLauncher mMockSyncConsentActivityLauncher;

    private @Nullable TemplateUrlService mActualTemplateUrlService;

    private MainSettings mMainSettings;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        InstrumentationRegistry.getInstrumentation().setInTouchMode(true);
        PasswordCheckFactory.setPasswordCheckForTesting(mPasswordCheck);
        SyncConsentActivityLauncherImpl.setLauncherForTest(mMockSyncConsentActivityLauncher);
        DeveloperSettings.setIsEnabledForTests(true);
        NightModeUtils.setNightModeSupportedForTesting(true);
    }

    @After
    public void tearDown() {
        DeveloperSettings.setIsEnabledForTests(null);
        NightModeUtils.setNightModeSupportedForTesting(null);
        if (mActualTemplateUrlService != null) {
            // Reset the actual service if the mock is used.
            TemplateUrlServiceFactory.setInstanceForTesting(mActualTemplateUrlService);
        }
        SharedPreferencesManager.getInstance().removeKey(
                SyncPromoController.getPromoShowCountPreferenceName(SigninAccessPoint.SETTINGS));
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT);
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @DisableFeatures({
            ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID,
            ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID_BRANDING,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ILLUSTRATION,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ALTERNATIVE_TITLE,
    })
    public void
    testRenderDifferentSignedInStates() throws IOException {
        launchSettingsActivity();
        waitForOptionsMenu();
        View view = mSettingsActivityTestRule.getActivity()
                            .findViewById(android.R.id.content)
                            .getRootView();
        mRenderTestRule.render(view, "main_settings_signed_out");

        // Sign in and render changes.
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForSyncFeatureActive();
        waitForOptionsMenu();
        // Waiting for sync to become active might take some time, so the scrollbar on the settings
        // view starts to fade, making the test flaky due to differences in the rendered image.
        // Sanitize the view to hide scrollbars (see https://crbug.com/1204117 for details).
        ThreadUtils.runOnUiThreadBlocking(() -> RenderTestRule.sanitize(view));
        mRenderTestRule.render(view, "main_settings_signed_in");
    }

    /**
     * Test for the "Account" section.
     *
     * TODO(crbug.com/1098205): remove code to explicitly enable Safety Check and Password check,
     * once the flags are on by default.
     */
    @Test
    @SmallTest
    @DisableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ILLUSTRATION,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
    })
    public void
    testStartupWithSyncPromoFeaturesDisabled() {
        launchSettingsActivity();

        // For non-signed-in users, the section contains the generic header.
        assertSettingsExists(MainSettings.PREF_SIGN_IN, null);
        Assert.assertTrue("Google services preference should be shown",
                mMainSettings.findPreference(MainSettings.PREF_GOOGLE_SERVICES).isVisible());

        // SignInPreference status check.
        // As the user is not signed in, sign in promo will show, section header and sync preference
        // will be hidden.
        Assert.assertFalse("Account section header should be hidden.",
                mMainSettings.findPreference(MainSettings.PREF_ACCOUNT_AND_GOOGLE_SERVICES_SECTION)
                        .isVisible());
        Assert.assertFalse("Sync preference should be hidden",
                mMainSettings.findPreference(MainSettings.PREF_MANAGE_SYNC).isVisible());

        // Assert for "Basics" section
        assertSettingsExists(MainSettings.PREF_SEARCH_ENGINE, SearchEngineSettings.class);
        assertSettingsExists(MainSettings.PREF_PASSWORDS, PasswordSettings.class);
        assertSettingsExists("autofill_payment_methods", AutofillPaymentMethodsFragment.class);
        assertSettingsExists("autofill_addresses", AutofillProfilesFragment.class);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            assertSettingsExists(MainSettings.PREF_NOTIFICATIONS, null);
        } else {
            Assert.assertNull("Notification setting should be hidden",
                    mMainSettings.findPreference(MainSettings.PREF_NOTIFICATIONS));
        }
        assertSettingsExists(MainSettings.PREF_HOMEPAGE, HomepageSettings.class);

        Preference themePref =
                assertSettingsExists(MainSettings.PREF_UI_THEME, ThemeSettingsFragment.class);
        Assert.assertEquals("ThemeSettingsEntry is missing.", ThemeSettingsEntry.SETTINGS,
                themePref.getExtras().getInt(ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY));

        // Verification for summary for the search engine and the homepage
        Assert.assertEquals("Homepage summary is different than homepage state",
                mMainSettings.getString(R.string.text_on),
                mMainSettings.findPreference(MainSettings.PREF_HOMEPAGE).getSummary().toString());

        // Assert for advanced section
        assertSettingsExists("privacy", PrivacySettings.class);
        assertSettingsExists(MainSettings.PREF_SAFETY_CHECK, SafetyCheckSettingsFragment.class);
        assertSettingsExists("accessibility", AccessibilitySettings.class);
        assertSettingsExists("content_settings", SiteSettings.class);
        assertSettingsExists("languages", LanguageSettings.class);
        assertSettingsExists(MainSettings.PREF_DOWNLOADS, DownloadSettings.class);
        assertSettingsExists(MainSettings.PREF_DEVELOPER, DeveloperSettings.class);
        assertSettingsExists("about_chrome", AboutChromeSettings.class);
    }

    @Test
    @SmallTest
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ILLUSTRATION,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
    })
    public void
    testStartupWithSyncPromoFeaturesEnabled() {
        launchSettingsActivity();

        // For non-signed-in users, the section contains the generic header.
        assertSettingsExists(MainSettings.PREF_SIGN_IN, null);
        Assert.assertTrue("Google services preference should be shown",
                mMainSettings.findPreference(MainSettings.PREF_GOOGLE_SERVICES).isVisible());

        // SignInPreference status check.
        // As the user is not signed in, sign in promo and section header will show. Sync preference
        // will be hidden.
        Assert.assertTrue("Account section header should be visible.",
                mMainSettings.findPreference(MainSettings.PREF_ACCOUNT_AND_GOOGLE_SERVICES_SECTION)
                        .isVisible());
        Assert.assertFalse("Sync preference should be hidden",
                mMainSettings.findPreference(MainSettings.PREF_MANAGE_SYNC).isVisible());

        // Assert for "Basics" section
        assertSettingsExists(MainSettings.PREF_SEARCH_ENGINE, SearchEngineSettings.class);
        assertSettingsExists(MainSettings.PREF_PASSWORDS, PasswordSettings.class);
        assertSettingsExists("autofill_payment_methods", AutofillPaymentMethodsFragment.class);
        assertSettingsExists("autofill_addresses", AutofillProfilesFragment.class);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            assertSettingsExists(MainSettings.PREF_NOTIFICATIONS, null);
        } else {
            Assert.assertNull("Notification setting should be hidden",
                    mMainSettings.findPreference(MainSettings.PREF_NOTIFICATIONS));
        }
        assertSettingsExists(MainSettings.PREF_HOMEPAGE, HomepageSettings.class);

        Preference themePref =
                assertSettingsExists(MainSettings.PREF_UI_THEME, ThemeSettingsFragment.class);
        Assert.assertEquals("ThemeSettingsEntry is missing.", ThemeSettingsEntry.SETTINGS,
                themePref.getExtras().getInt(ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY));

        // Verification for summary for the search engine and the homepage
        Assert.assertEquals("Homepage summary is different than homepage state",
                mMainSettings.getString(R.string.text_on),
                mMainSettings.findPreference(MainSettings.PREF_HOMEPAGE).getSummary().toString());

        // Assert for advanced section
        assertSettingsExists("privacy", PrivacySettings.class);
        assertSettingsExists(MainSettings.PREF_SAFETY_CHECK, SafetyCheckSettingsFragment.class);
        assertSettingsExists("accessibility", AccessibilitySettings.class);
        assertSettingsExists("content_settings", SiteSettings.class);
        assertSettingsExists("languages", LanguageSettings.class);
        assertSettingsExists(MainSettings.PREF_DOWNLOADS, DownloadSettings.class);
        assertSettingsExists(MainSettings.PREF_DEVELOPER, DeveloperSettings.class);
        assertSettingsExists("about_chrome", AboutChromeSettings.class);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON)
    public void testSigninRowLaunchesSignInFlowForSignedOutAccounts() {
        // When there are no accounts, sync promo and the signin preference shows the same text.
        mSyncTestRule.addTestAccount();
        launchSettingsActivity();

        onView(withText(R.string.sync_promo_turn_on_sync)).perform(click());

        verify(mMockSyncConsentActivityLauncher)
                .launchActivityIfAllowed(
                        any(Activity.class), eq(SigninAccessPoint.SETTINGS_SYNC_OFF_ROW));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.TANGIBLE_SYNC,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON})
    public void
    testSigninRowLaunchesTangibleSignInFlowForSignedOutAccounts() {
        // When there are no accounts, sync promo and the signin preference shows the same text.
        mSyncTestRule.addTestAccount();
        launchSettingsActivity();

        onView(withText(R.string.sync_promo_turn_on_sync)).perform(click());

        onView(withText(R.string.signin_account_picker_dialog_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(R.string.signin_add_account_to_device))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSyncRowLaunchesSignInFlowForSignedInAccounts() {
        CoreAccountInfo accountInfo = mSyncTestRule.setUpAccountAndSignInForTesting();
        launchSettingsActivity();

        onView(withText(R.string.sync_category_title)).perform(click());

        verify(mMockSyncConsentActivityLauncher)
                .launchActivityForPromoDefaultFlow(any(Activity.class),
                        eq(SigninAccessPoint.SETTINGS_SYNC_OFF_ROW), eq(accountInfo.getEmail()));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.TANGIBLE_SYNC)
    public void testSyncRowLaunchesTangibleSignInFlowForSignedInAccounts() {
        CoreAccountInfo accountInfo = mSyncTestRule.setUpAccountAndSignInForTesting();
        launchSettingsActivity();

        onView(withText(R.string.sync_category_title)).perform(click());

        onView(withText(R.string.signin_account_picker_dialog_title))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
        onView(withText(accountInfo.getEmail())).inRoot(isDialog()).check(matches(isDisplayed()));
        onView(withText(R.string.signin_add_account_to_device))
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSyncRowSummaryWhenNoDataTypeSynced() {
        final SyncService syncService =
                TestThreadUtils.runOnUiThreadBlockingNoException(SyncService::get);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { syncService.setSelectedTypes(false, new HashSet<>()); });
        CoreAccountInfo account = mSyncTestRule.addTestAccount();
        SigninTestUtil.signinAndEnableSync(account, syncService);

        launchSettingsActivity();

        onView(withText(R.string.sync_data_types_off)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSafeBrowsingSecuritySectionUiFlagOn() {
        launchSettingsActivity();
        assertSettingsExists(MainSettings.PREF_PRIVACY, PrivacySettings.class);
        Assert.assertEquals(mMainSettings.getString(R.string.prefs_privacy_security),
                mMainSettings.findPreference(MainSettings.PREF_PRIVACY).getTitle().toString());
    }

    @Test
    @SmallTest
    public void testHomepageOff() {
        mHomepageTestRule.disableHomepageForTest();
        launchSettingsActivity();

        // Verification for summary for the search engine and the homepage
        Assert.assertEquals("Homepage summary is different than homepage state",
                mMainSettings.getString(R.string.text_off),
                mMainSettings.findPreference(MainSettings.PREF_HOMEPAGE).getSummary().toString());
    }

    @Test
    @SmallTest
    public void testSearchEngineDisabled() {
        Mockito.doReturn(false).when(mMockTemplateUrlService).isLoaded();
        configureMockSearchEngine();

        launchSettingsActivity();
        Preference searchEngineSettings =
                assertSettingsExists(MainSettings.PREF_SEARCH_ENGINE, SearchEngineSettings.class);
        // Verification for summary for the search engine and the homepage
        Assert.assertFalse("Search Engine preference should be disabled when service is not ready.",
                searchEngineSettings.isEnabled());
        Assert.assertTrue("Search Engine preference should be disabled when service is not ready.",
                TextUtils.isEmpty(searchEngineSettings.getSummary()));
    }

    @Test
    @SmallTest
    @EnableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ILLUSTRATION,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
    })
    public void
    testAccountSignInWithNewPromo() throws InterruptedException {
        launchSettingsActivity();

        SyncPromoPreference syncPromoPreference =
                (SyncPromoPreference) mMainSettings.findPreference(MainSettings.PREF_SYNC_PROMO);
        Assert.assertEquals(
                "SyncPromoPreference should be at the personalized signin promo state. ",
                syncPromoPreference.getState(), State.PERSONALIZED_SIGNIN_PROMO);
        Assert.assertTrue("Account section header should be shown together with the promo.",
                mMainSettings.findPreference(MainSettings.PREF_ACCOUNT_AND_GOOGLE_SERVICES_SECTION)
                        .isVisible());
        Assert.assertFalse("Sync preference should be hidden when promo is shown.",
                mMainSettings.findPreference(MainSettings.PREF_MANAGE_SYNC).isVisible());

        CoreAccountInfo account = mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForSyncFeatureActive();
        Assert.assertEquals("SignInPreference should be at the signed in state.",
                account.getEmail(),
                mMainSettings.findPreference(MainSettings.PREF_SIGN_IN).getSummary().toString());
        assertSettingsExists(MainSettings.PREF_SIGN_IN, AccountManagementFragment.class);

        Assert.assertTrue("Account section header should be shown when user signed in.",
                mMainSettings.findPreference(MainSettings.PREF_ACCOUNT_AND_GOOGLE_SERVICES_SECTION)
                        .isVisible());
        Assert.assertTrue("Sync preference should be shown when the user is signed in.",
                mMainSettings.findPreference(MainSettings.PREF_MANAGE_SYNC).isVisible());
    }

    /**
     * Test when the sign-in preference is the promo. The section header should be hidden.
     */
    @Test
    @SmallTest
    @DisableFeatures({
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_ILLUSTRATION,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_SINGLE_BUTTON,
            ChromeFeatureList.SYNC_ANDROID_PROMOS_WITH_TITLE,
    })
    public void
    testAccountSignIn() throws InterruptedException {
        launchSettingsActivity();

        SyncPromoPreference syncPromoPreference =
                (SyncPromoPreference) mMainSettings.findPreference(MainSettings.PREF_SYNC_PROMO);
        Assert.assertEquals(
                "SyncPromoPreference should be at the personalized signin promo state. ",
                syncPromoPreference.getState(), State.PERSONALIZED_SIGNIN_PROMO);
        Assert.assertFalse("Account section header should be hidden when promo is shown.",
                mMainSettings.findPreference(MainSettings.PREF_ACCOUNT_AND_GOOGLE_SERVICES_SECTION)
                        .isVisible());
        Assert.assertFalse("Sync preference should be hidden when promo is shown.",
                mMainSettings.findPreference(MainSettings.PREF_MANAGE_SYNC).isVisible());

        // SignIn to see the changes
        CoreAccountInfo account = mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForSyncFeatureActive();
        // SignInPreference should be at the signed in state
        Assert.assertEquals("SignInPreference should be at the signed in state.",
                account.getEmail(),
                mMainSettings.findPreference(MainSettings.PREF_SIGN_IN).getSummary().toString());
        assertSettingsExists(MainSettings.PREF_SIGN_IN, AccountManagementFragment.class);

        Assert.assertTrue("Account section header should appear when user signed in.",
                mMainSettings.findPreference(MainSettings.PREF_ACCOUNT_AND_GOOGLE_SERVICES_SECTION)
                        .isVisible());
        Assert.assertTrue("Sync preference should appear when the user is signed in.",
                mMainSettings.findPreference(MainSettings.PREF_MANAGE_SYNC).isVisible());
    }

    @Test
    @SmallTest
    public void testRemoveSettings() {
        // Disable night mode
        NightModeUtils.setNightModeSupportedForTesting(false);

        // Disable developer option
        DeveloperSettings.setIsEnabledForTests(false);

        launchSettingsActivity();

        Assert.assertNull("Preference should be disabled: " + MainSettings.PREF_UI_THEME,
                mMainSettings.findPreference(MainSettings.PREF_UI_THEME));
        Assert.assertNull("Preference should be disabled: " + MainSettings.PREF_DEVELOPER,
                mMainSettings.findPreference(MainSettings.PREF_DEVELOPER));
    }

    @Test
    @SmallTest
    public void testDestroysPasswordCheck() {
        launchSettingsActivity();
        Activity activity = mMainSettings.getActivity();
        activity.finish();
        CriteriaHelper.pollUiThread(() -> activity.isDestroyed());
        Assert.assertNull(PasswordCheckFactory.getPasswordCheckInstance());
    }

    @Test
    @MediumTest
    public void testSyncPromoNotShownAfterBeingDismissed() throws Exception {
        HistogramDelta dismissedHistogram =
                new HistogramDelta("Signin.SyncPromo.Dismissed.Count.Settings", 1);
        launchSettingsActivity();
        onViewWaiting(allOf(withId(R.id.signin_promo_view_container), isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).perform(click());
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());

        // Close settings activity.
        Activity activity = mMainSettings.getActivity();
        ApplicationTestUtils.finishActivity(activity);

        // Launch settings activity again.
        mSettingsActivityTestRule.startSettingsActivity();
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
        Assert.assertEquals(1, dismissedHistogram.getDelta());
    }

    @Test
    @MediumTest
    public void testSyncPromoShownIsNotOverCounted() {
        HistogramDelta showCountHistogram =
                new HistogramDelta("Signin.SyncPromo.Shown.Count.Settings", 1);
        int promoShowCount = SharedPreferencesManager.getInstance().readInt(
                SyncPromoController.getPromoShowCountPreferenceName(SigninAccessPoint.SETTINGS));
        Assert.assertEquals(0, promoShowCount);
        Assert.assertEquals(0,
                SharedPreferencesManager.getInstance().readInt(
                        ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT));
        launchSettingsActivity();
        onViewWaiting(allOf(withId(R.id.signin_promo_view_container), isDisplayed()));

        promoShowCount = SharedPreferencesManager.getInstance().readInt(
                SyncPromoController.getPromoShowCountPreferenceName(SigninAccessPoint.SETTINGS));
        Assert.assertEquals(1, promoShowCount);
        Assert.assertEquals(1,
                SharedPreferencesManager.getInstance().readInt(
                        ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT));
        Assert.assertEquals(1, showCountHistogram.getDelta());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID,
            SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID})
    // Setting BrowserSignin suppresses the sync promo so the password settings preference
    // is visible without scrolling.
    @Policies.Add({
        @Policies.Item(key = "PasswordManagerEnabled", string = "false")
        , @Policies.Item(key = "BrowserSignin", string = "0")
    })
    public void
    testPasswordsItemClickableWhenManaged_EnableHighlightManagedPrefDisclaimerAndroid() {
        passwordsItemClickableWhenManaged();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    @DisableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    // Setting BrowserSignin suppresses the sync promo so the password settings preference
    // is visible without scrolling.
    @Policies.Add({
        @Policies.Item(key = "PasswordManagerEnabled", string = "false")
        , @Policies.Item(key = "BrowserSignin", string = "0")
    })
    public void
    testPasswordsItemClickableWhenManaged_DisableHighlightManagedPrefDisclaimerAndroid() {
        passwordsItemClickableWhenManaged();
    }

    public void passwordsItemClickableWhenManaged() {
        launchSettingsActivity();
        String prefTitleWithoutNewLabel =
                SpanApplier
                        .removeSpanText(
                                mMainSettings.getString(R.string.password_settings_title_gpm),
                                new SpanInfo("<new>", "</new>"))
                        .trim();
        if (SettingsFeatureList.isEnabled(
                    SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)) {
            onData(withKey(MainSettings.PREF_PASSWORDS))
                    .inAdapterView(allOf(isDisplayed(),
                            hasDescendant(withText(prefTitleWithoutNewLabel)),
                            hasDescendant(allOf(withText(R.string.managed_by_your_organization),
                                    isDisplayed()))));
        } else {
            onViewWaiting(allOf(withText(R.string.managed_by_your_organization),
                    hasSibling(withText(prefTitleWithoutNewLabel)), isDisplayed()));
        }
        Assert.assertTrue(mMainSettings.findPreference(MainSettings.PREF_PASSWORDS).isEnabled());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID,
            SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID})
    @Policies.Remove({ @Policies.Item(key = "PasswordManagerEnabled", string = "false") })
    // Setting BrowserSignin suppresses the sync promo so the password settings preference
    // is visible without scrolling.
    @Policies.Add(@Policies.Item(key = "BrowserSignin", string = "0"))
    public void
    testPasswordsItemEnabledWhenNotManaged_EnableHighlightManagedPrefDisclaimerAndroid()
            throws InterruptedException {
        passwordsItemEnabledWhenNotManaged();
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    @DisableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    @Policies.Remove({ @Policies.Item(key = "PasswordManagerEnabled", string = "false") })
    // Setting BrowserSignin suppresses the sync promo so the password settings preference
    // is visible without scrolling.
    @Policies.Add(@Policies.Item(key = "BrowserSignin", string = "0"))
    public void
    testPasswordsItemEnabledWhenNotManaged_DisableHighlightManagedPrefDisclaimerAndroid()
            throws InterruptedException {
        passwordsItemEnabledWhenNotManaged();
    }

    public void passwordsItemEnabledWhenNotManaged() throws InterruptedException {
        launchSettingsActivity();
        String prefTitleWithoutNewLabel =
                SpanApplier
                        .removeSpanText(
                                mMainSettings.getString(R.string.password_settings_title_gpm),
                                new SpanInfo("<new>", "</new>"))
                        .trim();
        if (SettingsFeatureList.isEnabled(
                    SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)) {
            onData(withKey(MainSettings.PREF_PASSWORDS))
                    .inAdapterView(allOf(isDisplayed(),
                            hasDescendant(withText(prefTitleWithoutNewLabel)),
                            hasDescendant(allOf(withText(R.string.managed_by_your_organization),
                                    not(isDisplayed())))));
        } else {
            onViewWaiting(allOf(withText(prefTitleWithoutNewLabel),
                    not(hasSibling(
                            allOf(withText(R.string.managed_by_your_organization), isDisplayed()))),
                    isDisplayed()));
        }
        Assert.assertTrue(mMainSettings.findPreference(MainSettings.PREF_PASSWORDS).isEnabled());
    }

    @Test
    @SmallTest
    @EnableFeatures(SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)
    @DisableFeatures({ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID,
            ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID_BRANDING})
    // Setting BrowserSignin suppresses the sync promo so the password settings preference
    // is visible without scrolling.
    @Policies.Add({
        @Policies.Item(key = "PasswordManagerEnabled", string = "false")
        , @Policies.Item(key = "BrowserSignin", string = "0")
    })
    public void
    testPasswordsItemEnabledWhenManagedWithoutUPM_EnableHighlightManagedPrefDisclaimerAndroid() {
        passwordsItemEnabledWhenManagedWithoutUPM();
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID,
            ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID_BRANDING,
            SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID})
    // Setting BrowserSignin suppresses the sync promo so the password settings preference
    // is visible without scrolling.
    @Policies.Add({
        @Policies.Item(key = "PasswordManagerEnabled", string = "false")
        , @Policies.Item(key = "BrowserSignin", string = "0")
    })
    public void
    testPasswordsItemEnabledWhenManagedWithoutUPM_DisableHighlightManagedPrefDisclaimerAndroid() {
        passwordsItemEnabledWhenManagedWithoutUPM();
    }

    public void passwordsItemEnabledWhenManagedWithoutUPM() {
        launchSettingsActivity();
        if (SettingsFeatureList.isEnabled(
                    SettingsFeatureList.HIGHLIGHT_MANAGED_PREF_DISCLAIMER_ANDROID)) {
            onData(withKey(MainSettings.PREF_PASSWORDS))
                    .inAdapterView(allOf(isDisplayed(),
                            hasDescendant(withText(R.string.password_settings_title)),
                            hasDescendant(allOf(withText(R.string.managed_by_your_organization),
                                    not(isDisplayed())))));
        } else {
            onViewWaiting(allOf(withText(R.string.password_settings_title),
                    not(hasSibling(
                            allOf(withText(R.string.managed_by_your_organization), isDisplayed()))),
                    isDisplayed()));
        }
        Assert.assertTrue(mMainSettings.findPreference(MainSettings.PREF_PASSWORDS).isEnabled());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID)
    public void testPasswordsItemTitleUpdatedWithUPM() throws InterruptedException {
        launchSettingsActivity();
        // TODO(crbug.com/1217070): Remove the New label checks once the feature is stable.
        String prefTitleWithoutNewLabel =
                SpanApplier
                        .removeSpanText(
                                mMainSettings.getString(R.string.password_settings_title_gpm),
                                new SpanInfo("<new>", "</new>"))
                        .toString()
                        .trim();
        Assert.assertEquals(prefTitleWithoutNewLabel,
                mMainSettings.findPreference(MainSettings.PREF_PASSWORDS).getTitle().toString());

        // Turn on sync to check if the "New" label is shown for sync users.
        CoreAccountInfo account = mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForSyncFeatureActive();

        String prefTitleWithNewLabel =
                SpanApplier
                        .applySpans(mMainSettings.getString(R.string.password_settings_title_gpm),
                                new SpanInfo("<new>", "</new>"))
                        .toString();
        Assert.assertEquals(prefTitleWithNewLabel,
                mMainSettings.findPreference(MainSettings.PREF_PASSWORDS).getTitle().toString());
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID,
            ChromeFeatureList.UNIFIED_PASSWORD_MANAGER_ANDROID_BRANDING})
    public void
    testPasswordsItemTitleNotUpdatedWithoutUPM() throws InterruptedException {
        launchSettingsActivity();
        Assert.assertEquals(mMainSettings.getString(R.string.password_settings_title),
                mMainSettings.findPreference(MainSettings.PREF_PASSWORDS).getTitle().toString());
    }

    private void launchSettingsActivity() {
        mSettingsActivityTestRule.startSettingsActivity();
        mMainSettings = mSettingsActivityTestRule.getFragment();
        Assert.assertNotNull("SettingsActivity failed to launch.", mMainSettings);
    }

    private void configureMockSearchEngine() {
        // Cache the actual Url Service, so the test can put it back after tests.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mActualTemplateUrlService = TemplateUrlServiceFactory.get(); });

        TemplateUrlServiceFactory.setInstanceForTesting(mMockTemplateUrlService);
        Mockito.doReturn(mMockSearchEngine)
                .when(mMockTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();

        Mockito.doReturn(SEARCH_ENGINE_SHORT_NAME).when(mMockSearchEngine).getShortName();
    }

    private void waitForOptionsMenu() {
        CriteriaHelper.pollUiThread(() -> {
            return mSettingsActivityTestRule.getActivity().findViewById(R.id.menu_id_general_help)
                    != null;
        });
    }

    /**
     * Assert the target preference exists in the main settings and creates the expected fragment,
     * then return that preference.
     *
     * @param prefKey preference key for {@link
     *         androidx.preference.PreferenceFragmentCompat#findPreference(CharSequence)}
     * @param settingsFragmentClass class name that the target preference is holding
     * @return the target preference if exists, raise {@link AssertionError} otherwise.
     */
    private Preference assertSettingsExists(String prefKey, @Nullable Class settingsFragmentClass) {
        Preference pref = mMainSettings.findPreference(prefKey);
        Assert.assertNotNull("Preference is missing: " + prefKey, pref);

        if (settingsFragmentClass == null) return pref;

        try {
            Assert.assertNotNull(
                    "Fragment attached to the preference is null.", pref.getFragment());
            Assert.assertEquals("Preference class is different.", settingsFragmentClass,
                    Class.forName(pref.getFragment()));
        } catch (ClassNotFoundException e) {
            throw new AssertionError("Pref fragment <" + pref.getFragment() + "> is not found.");
        }
        return pref;
    }
}
