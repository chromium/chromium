// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.support.test.InstrumentationRegistry;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.LargeTest;
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

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.about_settings.AboutChromeSettings;
import org.chromium.chrome.browser.accessibility.settings.AccessibilitySettings;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillProfilesFragment;
import org.chromium.chrome.browser.datareduction.settings.DataReductionPreferenceFragment;
import org.chromium.chrome.browser.download.settings.DownloadSettings;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.language.settings.LanguageSettings;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.notifications.settings.NotificationSettings;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_manager.settings.PasswordSettings;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineSettings;
import org.chromium.chrome.browser.signin.SigninActivityLauncherImpl;
import org.chromium.chrome.browser.signin.ui.SigninActivityLauncher;
import org.chromium.chrome.browser.sync.ProfileSyncService;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.sync.settings.SignInPreference;
import org.chromium.chrome.browser.sync.settings.SyncAndServicesSettings;
import org.chromium.chrome.browser.sync.settings.SyncPromoPreference;
import org.chromium.chrome.browser.sync.settings.SyncPromoPreference.State;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.IOException;
import java.util.HashSet;

/**
 * Test for {@link MainSettings}. Main purpose is to have a sanity check on the xml.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "show-autofill-signatures"})
public class MainSettingsFragmentTest {
    private static final String SEARCH_ENGINE_SHORT_NAME = "Google";

    private static final int RENDER_TEST_REVISION = 2;

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
    public ChromeRenderTestRule mRenderTestRule = ChromeRenderTestRule.Builder.withPublicCorpus()
                                                          .setRevision(RENDER_TEST_REVISION)
                                                          .build();

    @Mock
    public TemplateUrlService mMockTemplateUrlService;
    @Mock
    public TemplateUrl mMockSearchEngine;

    @Mock
    private PasswordCheck mPasswordCheck;

    @Mock
    private SigninActivityLauncher mMockSigninActivityLauncher;

    private @Nullable TemplateUrlService mActualTemplateUrlService;

    private MainSettings mMainSettings;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        InstrumentationRegistry.getInstrumentation().setInTouchMode(true);
        PasswordCheckFactory.setPasswordCheckForTesting(mPasswordCheck);
        SigninActivityLauncherImpl.setLauncherForTest(mMockSigninActivityLauncher);
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

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
    public void testRenderDifferentSignedInStates() throws IOException {
        launchSettingsActivity();
        View view = mSettingsActivityTestRule.getActivity()
                            .findViewById(android.R.id.content)
                            .getRootView();
        mRenderTestRule.render(view, "main_settings_signed_out");

        // Sign in and render changes.
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForSyncFeatureActive();
        mRenderTestRule.render(view, "main_settings_signed_in");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
    public void testRenderDifferentSignedInStatesWithMobileIdentityConsistency()
            throws IOException {
        launchSettingsActivity();
        // Scroll to the middle of the page because the preference is supposed to be at the middle
        // of the page.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            RecyclerView recyclerView =
                    mSettingsActivityTestRule.getFragment().getView().findViewById(
                            R.id.recycler_view);
            recyclerView.scrollToPosition(recyclerView.getAdapter().getItemCount() / 2);
        });
        View view = mSettingsActivityTestRule.getActivity()
                            .findViewById(android.R.id.content)
                            .getRootView();
        mRenderTestRule.render(view, "main_settings_signed_out_mobile_identity_consistency");

        // Sign in and render changes.
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForSyncFeatureActive();
        mRenderTestRule.render(view, "main_settings_signed_in_mobile_identity_consistency");
    }

    /**
     * Test for the "Account" section.
     *
     * TODO(crbug.com/1098205): remove code to explicitly enable Safety Check and Password check,
     * once the flags are on by default.
     */
    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
    public void testStartup() {
        launchSettingsActivity();

        // For non-signed-in users, the section contains the generic header.
        assertSettingsExists(MainSettings.PREF_SIGN_IN, null);
        assertSettingsExists(MainSettings.PREF_SYNC_AND_SERVICES, SyncAndServicesSettings.class);

        // SignInPreference status check.
        // As the user is not signed in, sign in promo will show, section header will be hidden.
        Assert.assertFalse("Account section header should be hidden.",
                mMainSettings.findPreference(MainSettings.PREF_ACCOUNT_SECTION).isVisible());

        // Assert for "Basics" section
        assertSettingsExists(MainSettings.PREF_SEARCH_ENGINE, SearchEngineSettings.class);
        assertSettingsExists(MainSettings.PREF_PASSWORDS, PasswordSettings.class);
        assertSettingsExists("autofill_payment_methods", AutofillPaymentMethodsFragment.class);
        assertSettingsExists("autofill_addresses", AutofillProfilesFragment.class);
        assertSettingsExists(MainSettings.PREF_NOTIFICATIONS, NotificationSettings.class);
        assertSettingsExists(MainSettings.PREF_HOMEPAGE, HomepageSettings.class);
        assertSettingsExists(MainSettings.PREF_UI_THEME, ThemeSettingsFragment.class);

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
        assertSettingsExists(
                MainSettings.PREF_DATA_REDUCTION, DataReductionPreferenceFragment.class);
        assertSettingsExists(MainSettings.PREF_DOWNLOADS, DownloadSettings.class);
        assertSettingsExists(MainSettings.PREF_DEVELOPER, DeveloperSettings.class);
        assertSettingsExists("about_chrome", AboutChromeSettings.class);
    }

    /**
     * Test for the "Account" section.
     *
     * TODO(crbug.com/1098205): remove code to explicitly enable Safety Check and Password check,
     * once the flags are on by default.
     */
    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
    public void testStartupWithMobileIdentityConsistency() {
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
        assertSettingsExists(MainSettings.PREF_NOTIFICATIONS, NotificationSettings.class);
        assertSettingsExists(MainSettings.PREF_HOMEPAGE, HomepageSettings.class);
        assertSettingsExists(MainSettings.PREF_UI_THEME, ThemeSettingsFragment.class);

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
        assertSettingsExists(
                MainSettings.PREF_DATA_REDUCTION, DataReductionPreferenceFragment.class);
        assertSettingsExists(MainSettings.PREF_DOWNLOADS, DownloadSettings.class);
        assertSettingsExists(MainSettings.PREF_DEVELOPER, DeveloperSettings.class);
        assertSettingsExists("about_chrome", AboutChromeSettings.class);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
    public void testSyncRowLaunchesSignInFlowForSignedInAccounts() {
        CoreAccountInfo accountInfo = mSyncTestRule.setUpAccountAndSignInForTesting();
        launchSettingsActivity();

        onView(withText(R.string.sync_category_title)).perform(click());
        verify(mMockSigninActivityLauncher)
                .launchActivityForPromoDefaultFlow(any(Activity.class),
                        eq(SigninAccessPoint.SETTINGS), eq(accountInfo.getEmail()));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)
    public void testSyncRowSummaryWhenNoDataTypeSynced() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { ProfileSyncService.get().setChosenDataTypes(false, new HashSet<>()); });
        CoreAccountInfo account = mSyncTestRule.addTestAccount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { SigninTestUtil.signinAndEnableSync(account, ProfileSyncService.get()); });
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

    /**
     * Test when the sign-in preference is the promo. The section header should be hidden.
     */
    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY})
    public void testAccountSignIn() {
        launchSettingsActivity();

        SyncPromoPreference syncPromoPreference =
                (SyncPromoPreference) mMainSettings.findPreference(MainSettings.PREF_SYNC_PROMO);
        Assert.assertEquals(
                "SyncPromoPreference should be at the personalized signin promo state. ",
                syncPromoPreference.getState(), State.PERSONALIZED_SIGNIN_PROMO);
        Assert.assertFalse("Account section header should be hidden when promo is shown.",
                mMainSettings.findPreference(MainSettings.PREF_ACCOUNT_SECTION).isVisible());

        // SignIn to see the changes
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForSyncFeatureActive();
        SignInPreference signInPreference =
                (SignInPreference) assertSettingsExists(MainSettings.PREF_SIGN_IN, null);
        Assert.assertEquals("SignInPreference should be at the signed in state. ",
                signInPreference.getState(), SignInPreference.State.SIGNED_IN);
        Assert.assertNotNull("Account section header should appear when user signed in.",
                mMainSettings.findPreference(MainSettings.PREF_ACCOUNT_SECTION));
    }

    /**
     * Test when the sign-in preference is the promo. The section header should be hidden.
     */
    @Test
    @SmallTest
    @Features.EnableFeatures({ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY})
    public void testAccountSignInWithMobileIdentityConsistency() throws InterruptedException {
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
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForSyncFeatureActive();
        SignInPreference signInPreference =
                (SignInPreference) assertSettingsExists(MainSettings.PREF_SIGN_IN, null);
        Assert.assertEquals("SignInPreference should be at the signed in state. ",
                signInPreference.getState(), SignInPreference.State.SIGNED_IN);
        Assert.assertTrue("Account section header should appear when user signed in.",
                mMainSettings.findPreference(MainSettings.PREF_ACCOUNT_AND_GOOGLE_SERVICES_SECTION)
                        .isVisible());
        Assert.assertTrue("Sync preference should appear when the user is signed in.",
                mMainSettings.findPreference(MainSettings.PREF_MANAGE_SYNC).isVisible());
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @Features.EnableFeatures({ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY})
    public void testSyncPromoView() throws Exception {
        mSyncTestRule.setUpAccountAndSignInForTesting();
        launchSettingsActivity();

        Preference syncPromoPreference = mMainSettings.findPreference(MainSettings.PREF_SYNC_PROMO);
        CriteriaHelper.pollUiThread(() -> syncPromoPreference.isVisible());
        View syncPromoView = mMainSettings.getView().findViewById(R.id.signin_promo_view_container);
        mRenderTestRule.render(syncPromoView, "main_settings_sync_promo");
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
