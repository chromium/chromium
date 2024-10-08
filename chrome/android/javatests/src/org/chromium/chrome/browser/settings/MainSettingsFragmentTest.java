// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.RecyclerViewActions.scrollTo;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.matcher.PreferenceMatchers.withKey;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.os.Looper;
import android.provider.Settings;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.preference.Preference;
import androidx.test.espresso.contrib.RecyclerViewActions;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
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

import org.chromium.base.BuildInfo;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.about_settings.AboutChromeSettings;
import org.chromium.chrome.browser.accessibility.settings.AccessibilitySettings;
import org.chromium.chrome.browser.autofill.settings.AutofillPaymentMethodsFragment;
import org.chromium.chrome.browser.autofill.settings.AutofillProfilesFragment;
import org.chromium.chrome.browser.download.settings.DownloadSettings;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.homepage.HomepageTestRule;
import org.chromium.chrome.browser.homepage.settings.HomepageSettings;
import org.chromium.chrome.browser.language.settings.LanguageSettings;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigSettings;
import org.chromium.chrome.browser.night_mode.NightModeMetrics.ThemeSettingsEntry;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.password_check.PasswordCheck;
import org.chromium.chrome.browser.password_check.PasswordCheckFactory;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.password_manager.settings.PasswordSettings;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safety_check.SafetyCheckSettingsFragment;
import org.chromium.chrome.browser.safety_hub.SafetyHubFragment;
import org.chromium.chrome.browser.safety_hub.SafetyHubMetricUtils;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineSettings;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.signin.SyncConsentActivityLauncherImpl;
import org.chromium.chrome.browser.sync.FakeSyncServiceImpl;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.sync.settings.AccountManagementFragment;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SignInPreference;
import org.chromium.chrome.browser.sync.settings.SyncPromoPreference;
import org.chromium.chrome.browser.sync.settings.SyncPromoPreference.State;
import org.chromium.chrome.browser.tasks.tab_management.TabsSettings;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncCoordinator;
import org.chromium.chrome.browser.ui.signin.SyncConsentActivityLauncher;
import org.chromium.chrome.browser.ui.signin.SyncPromoController;
import org.chromium.chrome.browser.ui.signin.account_picker.AccountPickerBottomSheetStrings;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.SigninFeatures;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.AccountCapabilitiesBuilder;
import org.chromium.components.sync.SyncService;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;
import java.util.HashSet;

/** Test for {@link MainSettings}. Main purpose is to have a quick confidence check on the xml. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "show-autofill-signatures"})
@DoNotBatch(reason = "Tests cannot run batched because they launch a Settings activity.")
@EnableFeatures(SigninFeatures.HIDE_SETTINGS_SIGN_IN_PROMO)
public class MainSettingsFragmentTest {
    private static final String SEARCH_ENGINE_SHORT_NAME = "Google";

    private static final int RENDER_TEST_REVISION = 13;
    private static final String RENDER_TEST_DESCRIPTION =
            "Alert icon on identity error for signed in users";

    private final HomepageTestRule mHomepageTestRule = new HomepageTestRule();

    private final SyncTestRule mSyncTestRule = new SyncTestRule();

    private final SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(MainSettings.class);

    // SettingsActivity needs to be initialized and destroyed with the mock
    // signin environment setup in SyncTestRule
    @Rule
    public final RuleChain mRuleChain =
            RuleChain.outerRule(mSyncTestRule)
                    .around(mHomepageTestRule)
                    .around(mSettingsActivityTestRule);

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(RENDER_TEST_REVISION)
                    .setDescription(RENDER_TEST_DESCRIPTION)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_SETTINGS)
                    .build();

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock public TemplateUrlService mMockTemplateUrlService;
    @Mock public TemplateUrl mMockSearchEngine;

    @Mock private PasswordCheck mPasswordCheck;
    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;

    @Mock private SyncConsentActivityLauncher mSyncConsentActivityLauncher;
    @Mock private SigninAndHistorySyncActivityLauncher mSigninAndHistorySyncActivityLauncher;
    @Mock private HomeModulesConfigManager mHomeModulesConfigManager;

    @Mock private Tracker mTestTracker;
    @Mock private DefaultBrowserPromoUtils mMockDefaultBrowserPromoUtils;

    private MainSettings mMainSettings;

    @Before
    public void setup() {
        // ObservableSupplierImpl needs a Looper.
        Looper.prepare();
        MockitoAnnotations.initMocks(this);
        InstrumentationRegistry.getInstrumentation().setInTouchMode(true);
        PasswordCheckFactory.setPasswordCheckForTesting(mPasswordCheck);
        mJniMocker.mock(PasswordManagerUtilBridgeJni.TEST_HOOKS, mPasswordManagerUtilBridgeJniMock);
        SyncConsentActivityLauncherImpl.setLauncherForTest(mSyncConsentActivityLauncher);
        SigninAndHistorySyncActivityLauncherImpl.setLauncherForTest(
                mSigninAndHistorySyncActivityLauncher);
        DeveloperSettings.setIsEnabledForTests(true);
        NightModeUtils.setNightModeSupportedForTesting(true);
        Intents.init();
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance()
                .removeKey(
                        SyncPromoController.getPromoShowCountPreferenceName(
                                SigninAccessPoint.SETTINGS));
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT);
        Intents.release();
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "http://b/issues/41491395")
    public void testRenderDifferentSignedInStates() throws IOException {
        startSettings();
        waitForOptionsMenu();
        View view =
                mSettingsActivityTestRule
                        .getActivity()
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

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testRenderSignedOutAccountManagementRows_replaceSyncBySigninEnabled()
            throws IOException {
        startSettings();
        waitForOptionsMenu();

        View accountRow =
                mSettingsActivityTestRule
                        .getActivity()
                        .findViewById(R.id.account_management_account_row);
        mRenderTestRule.render(
                accountRow, "main_settings_signed_out_account_replace_sync_by_signin_enabled");
        View googleServicesRow =
                mSettingsActivityTestRule
                        .getActivity()
                        .findViewById(R.id.account_management_google_services_row);
        mRenderTestRule.render(
                googleServicesRow,
                "main_settings_signed_out_google_services_replace_sync_by_signin_enabled");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @Policies.Add({@Policies.Item(key = "BrowserSignin", string = "0")})
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testRenderSigninDisabledByPolicyAccountRow_replaceSyncBySigninEnabled()
            throws IOException {
        startSettings();
        waitForOptionsMenu();

        View accountRow =
                mSettingsActivityTestRule
                        .getActivity()
                        .findViewById(R.id.account_management_account_row);
        mRenderTestRule.render(
                accountRow,
                "main_settings_signin_disabled_by_policy_account_replace_sync_by_signin_enabled");
    }

    /**
     * Test for the "Account" section.
     *
     * <p>TODO(b/324562205): update to check for Safety Hub instead of Safety Check once it's fully
     * launched.
     */
    @Test
    @SmallTest
    @EnableFeatures(AutofillFeatures.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID)
    @DisableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testStartup() {
        startSettings();

        // For non-signed-in users, the section contains the generic header.
        assertSettingsExists(MainSettings.PREF_SIGN_IN, null);
        Assert.assertTrue(
                "Google services preference should be shown",
                mMainSettings.findPreference(MainSettings.PREF_GOOGLE_SERVICES).isVisible());

        // SignInPreference status check.
        // As the user is not signed in, sign in promo and section header will show. Sync preference
        // will be hidden.
        Assert.assertTrue(
                "Account section header should be visible.",
                mMainSettings
                        .findPreference(MainSettings.PREF_ACCOUNT_AND_GOOGLE_SERVICES_SECTION)
                        .isVisible());
        Assert.assertFalse(
                "Sync preference should be hidden",
                mMainSettings.findPreference(MainSettings.PREF_MANAGE_SYNC).isVisible());

        // Assert for "Basics" section
        assertSettingsExists(MainSettings.PREF_SEARCH_ENGINE, SearchEngineSettings.class);
        if (supportThirdPartyFillingSetting()) {
            assertSettingsExists(MainSettings.PREF_AUTOFILL_OPTIONS, null);
            assertSettingsExists(MainSettings.PREF_AUTOFILL_SECTION, null);
        } else {
            Assert.assertNull(
                    "Third party filling setting should be hidden",
                    mMainSettings.findPreference(MainSettings.PREF_AUTOFILL_OPTIONS));
            Assert.assertNull(
                    "Autofill section header should be hidden",
                    mMainSettings.findPreference(MainSettings.PREF_AUTOFILL_SECTION));
        }
        assertSettingsExists(MainSettings.PREF_PASSWORDS, PasswordSettings.class);
        assertSettingsExists("autofill_payment_methods", AutofillPaymentMethodsFragment.class);
        assertSettingsExists("autofill_addresses", AutofillProfilesFragment.class);
        if (supportNotificationSettings()) {
            assertSettingsExists(MainSettings.PREF_NOTIFICATIONS, null);
        } else {
            Assert.assertNull(
                    "Notification setting should be hidden",
                    mMainSettings.findPreference(MainSettings.PREF_NOTIFICATIONS));
        }
        assertSettingsExists(MainSettings.PREF_HOMEPAGE, HomepageSettings.class);

        Preference themePref =
                assertSettingsExists(MainSettings.PREF_UI_THEME, ThemeSettingsFragment.class);
        Assert.assertEquals(
                "ThemeSettingsEntry is missing.",
                ThemeSettingsEntry.SETTINGS,
                themePref.getExtras().getInt(ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY));

        // Verification for summary for the search engine and the homepage
        Assert.assertEquals(
                "Homepage summary is different than homepage state",
                mMainSettings.getString(R.string.text_on),
                mMainSettings.findPreference(MainSettings.PREF_HOMEPAGE).getSummary().toString());

        // Assert for advanced section
        assertSettingsExists("privacy", PrivacySettings.class);
        if (BuildInfo.getInstance().isAutomotive) {
            Assert.assertNull(
                    "Safety check should not be shown on automotive",
                    mMainSettings.findPreference(MainSettings.PREF_SAFETY_CHECK));
        } else {
            assertSettingsExists(MainSettings.PREF_SAFETY_CHECK, SafetyCheckSettingsFragment.class);
        }
        assertSettingsExists("accessibility", AccessibilitySettings.class);
        assertSettingsExists("content_settings", SiteSettings.class);
        assertSettingsExists("languages", LanguageSettings.class);
        assertSettingsExists(MainSettings.PREF_DOWNLOADS, DownloadSettings.class);
        assertSettingsExists(MainSettings.PREF_DEVELOPER, DeveloperSettings.class);
        assertSettingsExists("about_chrome", AboutChromeSettings.class);
    }

    @Test
    @SmallTest
    @DisableFeatures({
        ChromeFeatureList.SAFETY_HUB,
        AutofillFeatures.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID
    })
    public void testLegacyOrderRemainsConsistent() {
        startSettings();
        @Nullable Preference prevPref = null;
        for (int i = 0; i < mMainSettings.getPreferenceScreen().getPreferenceCount(); ++i) {
            Preference pref = mMainSettings.getPreferenceScreen().getPreference(i);
            if (!pref.isShown()) { // Skip invisible prefs.
                continue;
            }
            if (prevPref == null) { // Skip first pref.
                prevPref = pref;
                continue;
            }
            assertTrue(
                    prevPref.getTitle() + " should precede " + pref.getTitle(),
                    pref.getOrder() > prevPref.getOrder());
        }
    }

    @Test
    @SmallTest
    @EnableFeatures(AutofillFeatures.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID)
    @DisableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testConsistentOrder() {
        startSettings();
        @Nullable Preference prevPref = null;
        for (int i = 0; i < mMainSettings.getPreferenceScreen().getPreferenceCount(); ++i) {
            Preference pref = mMainSettings.getPreferenceScreen().getPreference(i);
            if (!pref.isShown()) { // Skip invisible prefs.
                continue;
            }
            if (prevPref == null) { // Skip first pref.
                prevPref = pref;
                continue;
            }
            assertTrue(
                    prevPref.getTitle() + " should precede " + pref.getTitle(),
                    pref.getOrder() > prevPref.getOrder());
        }
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testSignInRowLaunchesSyncFlowForSignedOutAccounts() {
        // When there are no accounts, sync promo and the signin preference shows the same text.
        mSyncTestRule.addTestAccount();
        startSettings();

        onViewWaiting(allOf(withId(R.id.recycler_view), isDisplayed()));
        onView(withId(R.id.recycler_view))
                .perform(scrollTo(hasDescendant(withText(R.string.sync_promo_turn_on_sync))));
        onView(withText(R.string.sync_promo_turn_on_sync)).perform(click());

        verify(mSyncConsentActivityLauncher)
                .launchActivityIfAllowed(
                        any(Activity.class), eq(SigninAccessPoint.SETTINGS_SYNC_OFF_ROW));
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testPressingSignOut() {
        CoreAccountInfo accountInfo = mSyncTestRule.setUpAccountAndSignInForTesting();

        startSettings();

        onView(withText(accountInfo.getEmail())).perform(click());
        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        onView(withText(R.string.sign_out)).perform(click());
        Assert.assertNull(mSyncTestRule.getSigninTestRule().getPrimaryAccount(ConsentLevel.SIGNIN));

        Activity activity = mSettingsActivityTestRule.getActivity();
        final String expectedSnackbarMessage =
                activity.getString(R.string.sign_out_snackbar_message);
        CriteriaHelper.pollUiThread(
                () -> {
                    SnackbarManager snackbarManager =
                            ((SnackbarManager.SnackbarManageable) activity).getSnackbarManager();
                    Criteria.checkThat(snackbarManager.isShowing(), Matchers.is(true));
                    TextView snackbarMessage = activity.findViewById(R.id.snackbar_message);
                    Criteria.checkThat(snackbarMessage, Matchers.notNullValue());
                    Criteria.checkThat(
                            snackbarMessage.getText().toString(),
                            Matchers.is(expectedSnackbarMessage));
                });
    }

    @Test
    @LargeTest
    @Feature({"Sync"})
    @EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
    public void testPressingTurnOffSyncWhileTheUNOFlagIsEnabled() {
        mSyncTestRule.setUpChildAccountAndEnableSyncForTesting();

        startSettings();

        onView(withText(R.string.sync_category_title)).perform(click());
        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        onView(withText(R.string.turn_off_sync)).perform(click());
        onView(withText(R.string.continue_button)).perform(click());
        Assert.assertNull(mSyncTestRule.getSigninTestRule().getPrimaryAccount(ConsentLevel.SYNC));
        Assert.assertNotNull(
                mSyncTestRule.getSigninTestRule().getPrimaryAccount(ConsentLevel.SIGNIN));

        Activity activity = mSettingsActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> {
                    SnackbarManager snackbarManager =
                            ((SnackbarManager.SnackbarManageable) activity).getSnackbarManager();
                    Criteria.checkThat(snackbarManager.isShowing(), Matchers.is(false));
                    TextView snackbarMessage = activity.findViewById(R.id.snackbar_message);
                    Criteria.checkThat(snackbarMessage, Matchers.nullValue());
                });
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testSignInRowLaunchesSignInFlowForSignedOutAccounts() {
        mSyncTestRule.addTestAccount();
        startSettings();

        onView(withId(R.id.recycler_view))
                .perform(scrollTo(hasDescendant(withText(R.string.signin_settings_title))));
        onView(withText(R.string.signin_settings_subtitle)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_settings_title)).perform(click());

        verify(mSigninAndHistorySyncActivityLauncher)
                .launchActivityIfAllowed(
                        any(Activity.class),
                        any(Profile.class),
                        any(AccountPickerBottomSheetStrings.class),
                        eq(SigninAndHistorySyncCoordinator.NoAccountSigninMode.BOTTOM_SHEET),
                        eq(
                                SigninAndHistorySyncCoordinator.WithAccountSigninMode
                                        .DEFAULT_ACCOUNT_BOTTOM_SHEET),
                        eq(SigninAndHistorySyncCoordinator.HistoryOptInMode.OPTIONAL),
                        eq(SigninAccessPoint.SETTINGS));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testSyncRowLaunchesSignInFlowForSignedInAccounts() {
        CoreAccountInfo accountInfo = mSyncTestRule.setUpAccountAndSignInForTesting();
        startSettings();

        onViewWaiting(allOf(withId(R.id.recycler_view), isDisplayed()));
        onView(withId(R.id.recycler_view))
                .perform(scrollTo(hasDescendant(withText(R.string.sync_category_title))));
        onView(withText(R.string.sync_category_title)).perform(click());

        verify(mSyncConsentActivityLauncher)
                .launchActivityForPromoDefaultFlow(
                        any(Activity.class),
                        eq(SigninAccessPoint.SETTINGS_SYNC_OFF_ROW),
                        eq(accountInfo.getEmail()));
    }

    // Tests that no alert icon is visible if there are no identity errors.
    @Test
    @SmallTest
    public void testSigninRowShowsNoAlertWhenNoIdentityErrors() {
        // Sign-in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        startSettings();

        assertSettingsExists(
                MainSettings.PREF_SIGN_IN,
                ChromeFeatureList.isEnabled(
                                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                        ? ManageSyncSettings.class
                        : AccountManagementFragment.class);
        onView(allOf(withId(R.id.alert_icon), isDisplayed())).check(doesNotExist());
    }

    // Tests that no alert icon is shown on the account row for syncing users, even if there exists
    // an identity error.
    @Test
    @SmallTest
    public void testSigninRowShowsNoAlertForIdentityErrorsForSyncingUsers() {
        FakeSyncServiceImpl fakeSyncService =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            FakeSyncServiceImpl fakeSyncServiceImpl = new FakeSyncServiceImpl();
                            SyncServiceFactory.setInstanceForTesting(fakeSyncServiceImpl);
                            return fakeSyncServiceImpl;
                        });
        // Fake an identity error.
        fakeSyncService.setRequiresClientUpgrade(true);
        // Sign-in and enable sync. Open settings.
        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        startSettings();

        assertSettingsExists(MainSettings.PREF_SIGN_IN, AccountManagementFragment.class);
        onView(allOf(withId(R.id.alert_icon), isDisplayed())).check(doesNotExist());
    }

    // Tests that an alert icon is shown on the account row in case of an identity error for a
    // signed-in non-syncing user.
    @Test
    @SmallTest
    public void testSigninRowShowsAlertForIdentityErrors() {
        FakeSyncServiceImpl fakeSyncService =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            FakeSyncServiceImpl fakeSyncServiceImpl = new FakeSyncServiceImpl();
                            SyncServiceFactory.setInstanceForTesting(fakeSyncServiceImpl);
                            return fakeSyncServiceImpl;
                        });
        // Fake an identity error.
        fakeSyncService.setRequiresClientUpgrade(true);
        // Sign in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        startSettings();

        assertSettingsExists(
                MainSettings.PREF_SIGN_IN,
                ChromeFeatureList.isEnabled(
                                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                        ? ManageSyncSettings.class
                        : AccountManagementFragment.class);
        onView(allOf(withId(R.id.alert_icon), isDisplayed())).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testRenderOnIdentityErrorForSignedInUsers_withoutReplaceSyncPromos()
            throws IOException {
        FakeSyncServiceImpl fakeSyncService =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            FakeSyncServiceImpl fakeSyncServiceImpl = new FakeSyncServiceImpl();
                            SyncServiceFactory.setInstanceForTesting(fakeSyncServiceImpl);
                            return fakeSyncServiceImpl;
                        });
        // Fake an identity error.
        fakeSyncService.setRequiresClientUpgrade(true);
        // Sign in and wait for sync machinery to be active.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        SyncTestUtil.waitForSyncTransportActive();

        startSettings();

        View view =
                mSettingsActivityTestRule
                        .getActivity()
                        .findViewById(android.R.id.content)
                        .getRootView();
        mRenderTestRule.render(view, "main_settings_signed_in_identity_error");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testRenderOnIdentityErrorForSignedInUsers_withReplaceSyncPromos()
            throws IOException {
        FakeSyncServiceImpl fakeSyncService =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            FakeSyncServiceImpl fakeSyncServiceImpl = new FakeSyncServiceImpl();
                            SyncServiceFactory.setInstanceForTesting(fakeSyncServiceImpl);
                            return fakeSyncServiceImpl;
                        });
        // Fake an identity error.
        fakeSyncService.setRequiresClientUpgrade(true);
        // Sign in and wait for sync machinery to be active.
        CoreAccountInfo accountInfo = mSyncTestRule.setUpAccountAndSignInForTesting();
        SyncTestUtil.waitForSyncTransportActive();

        startSettings();

        // Population of profile data is flaky. Thus, wait till it's populated.
        // TODO(crbug.com/40944114): Check if there exists an alternate way out.
        SignInPreference signInPreference = mMainSettings.findPreference(MainSettings.PREF_SIGN_IN);
        CriteriaHelper.pollUiThread(
                () -> {
                    return signInPreference
                            .getProfileDataCache()
                            .hasProfileDataForTesting(accountInfo.getEmail());
                });

        View view =
                mSettingsActivityTestRule
                        .getActivity()
                        .findViewById(android.R.id.content)
                        .getRootView();
        mRenderTestRule.render(
                view, "main_settings_signed_in_identity_error_with_replace_sync_promos");
    }

    @Test
    @SmallTest
    public void testSyncRowSummaryWhenNoDataTypeSynced() {
        CoreAccountInfo account = mSyncTestRule.addTestAccount();
        final SyncService syncService = SyncTestUtil.getSyncServiceForLastUsedProfile();
        SigninTestUtil.signinAndEnableSync(account, syncService);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    syncService.setSelectedTypes(false, new HashSet<>());
                });

        startSettings();

        onView(withText(R.string.sync_data_types_off)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSyncRowSummaryWhenUpmBackendOutdated() {
        when(mPasswordManagerUtilBridgeJniMock.isGmsCoreUpdateRequired(any(), any()))
                .thenReturn(true);

        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForSyncFeatureActive();

        startSettings();

        onViewWaiting(withText(R.string.sync_error_outdated_gms)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testSafeBrowsingSecuritySectionUiFlagOn() {
        startSettings();
        assertSettingsExists(MainSettings.PREF_PRIVACY, PrivacySettings.class);
        Assert.assertEquals(
                mMainSettings.getString(R.string.prefs_privacy_security),
                mMainSettings.findPreference(MainSettings.PREF_PRIVACY).getTitle().toString());
    }

    @Test
    @SmallTest
    public void testHomepageOff() {
        mHomepageTestRule.disableHomepageForTest();
        startSettings();

        // Verification for summary for the search engine and the homepage
        Assert.assertEquals(
                "Homepage summary is different than homepage state",
                mMainSettings.getString(R.string.text_off),
                mMainSettings.findPreference(MainSettings.PREF_HOMEPAGE).getSummary().toString());
    }

    @Test
    @SmallTest
    public void testSearchEngineDisabled() {
        Mockito.doReturn(false).when(mMockTemplateUrlService).isLoaded();
        configureMockSearchEngine();

        startSettings();
        Preference searchEngineSettings =
                assertSettingsExists(MainSettings.PREF_SEARCH_ENGINE, SearchEngineSettings.class);
        // Verification for summary for the search engine and the homepage
        Assert.assertFalse(
                "Search Engine preference should be disabled when service is not ready.",
                searchEngineSettings.isEnabled());
        Assert.assertTrue(
                "Search Engine preference should be disabled when service is not ready.",
                TextUtils.isEmpty(searchEngineSettings.getSummary()));
    }

    @Test
    @SmallTest
    @DisabledTest(message = "http://b/issues/41491395")
    public void testAccountSignIn() throws InterruptedException {
        startSettings();

        SyncPromoPreference syncPromoPreference =
                (SyncPromoPreference) mMainSettings.findPreference(MainSettings.PREF_SYNC_PROMO);
        Assert.assertEquals(
                "SyncPromoPreference should be at the personalized signin promo state. ",
                syncPromoPreference.getState(),
                State.PERSONALIZED_SIGNIN_PROMO);
        Assert.assertTrue(
                "Account section header should be shown together with the promo.",
                mMainSettings
                        .findPreference(MainSettings.PREF_ACCOUNT_AND_GOOGLE_SERVICES_SECTION)
                        .isVisible());
        Assert.assertFalse(
                "Sync preference should be hidden when promo is shown.",
                mMainSettings.findPreference(MainSettings.PREF_MANAGE_SYNC).isVisible());

        CoreAccountInfo account = mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForSyncFeatureActive();
        Assert.assertEquals(
                "SignInPreference should be at the signed in state.",
                account.getEmail(),
                mMainSettings.findPreference(MainSettings.PREF_SIGN_IN).getSummary().toString());
        assertSettingsExists(MainSettings.PREF_SIGN_IN, AccountManagementFragment.class);

        Assert.assertTrue(
                "Account section header should be shown when user signed in.",
                mMainSettings
                        .findPreference(MainSettings.PREF_ACCOUNT_AND_GOOGLE_SERVICES_SECTION)
                        .isVisible());
        Assert.assertTrue(
                "Sync preference should be shown when the user is signed in.",
                mMainSettings.findPreference(MainSettings.PREF_MANAGE_SYNC).isVisible());
    }

    @Test
    @MediumTest
    @EnableFeatures(SigninFeatures.HIDE_SETTINGS_SIGN_IN_PROMO)
    public void testSignInPromoHidden_HideSignInPromoEnabled() {
        startSettings();

        onView(withText(R.string.sync_promo_title_settings)).check(doesNotExist());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void
            testManageSyncRowIsNotShownWhenReplaceSyncPromosWithSignInPromosEnabledWithoutSyncConsent()
                    throws InterruptedException {
        startSettings();

        Assert.assertFalse(
                "Sync preference should be hidden when the user is signed out.",
                mMainSettings.findPreference(MainSettings.PREF_MANAGE_SYNC).isVisible());

        mSyncTestRule.setUpAccountAndSignInForTesting();
        Assert.assertFalse(
                "Sync preference should not be shown when the user is signed in.",
                mMainSettings.findPreference(MainSettings.PREF_MANAGE_SYNC).isVisible());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    @DisabledTest(message = "http://b/issues/41491395")
    public void
            testManageSyncRowIsShownWhenReplaceSyncPromosWithSignInPromosEnabledWithSyncConsent()
                    throws InterruptedException {
        startSettings();

        Assert.assertFalse(
                "Sync preference should be hidden when the user is signed out.",
                mMainSettings.findPreference(MainSettings.PREF_MANAGE_SYNC).isVisible());

        mSyncTestRule.setUpAccountAndEnableSyncForTesting();
        SyncTestUtil.waitForSyncFeatureActive();
        Assert.assertTrue(
                "Sync preference should be shown when the user is syncing.",
                mMainSettings.findPreference(MainSettings.PREF_MANAGE_SYNC).isVisible());
    }

    @Test
    @SmallTest
    public void testAccountManagementRowForChildAccountWithNonDisplayableAccountEmail()
            throws InterruptedException {
        startSettings();

        // Account set up.
        final SigninTestRule signinTestRule = mSyncTestRule.getSigninTestRule();
        AccountInfo accountInfo =
                signinTestRule.addChildTestAccountThenWaitForSignin(
                        new AccountCapabilitiesBuilder().setCanHaveEmailAddressDisplayed(false));

        // Force update the preference so that NON_DISPLAYABLE_EMAIL_ACCOUNT_CAPABILITIES is
        // actually utilized. This is to replicate downstream implementation behavior, where
        // checkIfDisplayableEmailAddress() differs.
        SignInPreference signInPreference = mMainSettings.findPreference(MainSettings.PREF_SIGN_IN);
        CriteriaHelper.pollUiThread(
                () -> {
                    return !signInPreference
                            .getProfileDataCache()
                            .getProfileDataOrDefault(accountInfo.getEmail())
                            .hasDisplayableEmailAddress();
                });
        ThreadUtils.runOnUiThreadBlocking(signInPreference::syncStateChanged);

        mSettingsActivityTestRule.startSettingsActivity();
        onView(allOf(withText(accountInfo.getFullName()), isDisplayed()))
                .check(matches(isDisplayed()));
        onView(withText(accountInfo.getEmail())).check(doesNotExist());
    }

    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/362211398")
    public void
            testAccountManagementRowForChildAccountWithNonDisplayableAccountEmailWithEmptyDisplayName()
                    throws InterruptedException {
        startSettings();

        // Account set up.
        // If both fullName and givenName are empty, accountCapabilities is ignored.
        final SigninTestRule signinTestRule = mSyncTestRule.getSigninTestRule();
        signinTestRule.addAccountThenSignin(
                AccountManagerTestRule.TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL_AND_NO_NAME);

        SignInPreference signInPreference = mMainSettings.findPreference(MainSettings.PREF_SIGN_IN);
        CriteriaHelper.pollUiThread(
                () -> {
                    return !signInPreference
                            .getProfileDataCache()
                            .getProfileDataOrDefault(
                                    AccountManagerTestRule
                                            .TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL_AND_NO_NAME
                                            .getEmail())
                            .hasDisplayableEmailAddress();
                });
        ThreadUtils.runOnUiThreadBlocking(signInPreference::syncStateChanged);

        mSettingsActivityTestRule.startSettingsActivity();

        onView(
                        withText(
                                AccountManagerTestRule
                                        .TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL_AND_NO_NAME
                                        .getEmail()))
                .check(doesNotExist());
        onView(allOf(withText(R.string.default_google_account_username), isDisplayed()))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testRemoveSettings() {
        // Disable night mode
        NightModeUtils.setNightModeSupportedForTesting(false);

        // Disable developer option
        DeveloperSettings.setIsEnabledForTests(false);

        startSettings();

        Assert.assertNull(
                "Preference should be disabled: " + MainSettings.PREF_UI_THEME,
                mMainSettings.findPreference(MainSettings.PREF_UI_THEME));
        Assert.assertNull(
                "Preference should be disabled: " + MainSettings.PREF_DEVELOPER,
                mMainSettings.findPreference(MainSettings.PREF_DEVELOPER));
    }

    @Test
    @SmallTest
    public void testDestroysPasswordCheck() {
        startSettings();
        Activity activity = mMainSettings.getActivity();
        activity.finish();
        CriteriaHelper.pollUiThread(() -> activity.isDestroyed());
        Assert.assertNull(PasswordCheckFactory.getPasswordCheckInstance());
    }

    @Test
    @MediumTest
    @DisableFeatures({
        SigninFeatures.HIDE_SETTINGS_SIGN_IN_PROMO,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS
    })
    public void testSyncPromoNotShownAfterBeingDismissed() throws Exception {
        var dismissedCountHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SyncPromo.Dismissed.Count.Settings", 1);
        startSettings();
        onViewWaiting(allOf(withId(R.id.signin_promo_view_container), isDisplayed()));
        onView(withId(R.id.sync_promo_close_button)).perform(click());
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());

        // Close settings activity.
        mSettingsActivityTestRule.finishActivity();

        // Launch settings activity again.
        mSettingsActivityTestRule.startSettingsActivity();
        onView(withId(R.id.signin_promo_view_container)).check(doesNotExist());
        dismissedCountHistogram.assertExpected();
    }

    @Test
    @MediumTest
    @DisableFeatures({
        SigninFeatures.HIDE_SETTINGS_SIGN_IN_PROMO,
        ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS
    })
    public void testSyncPromoShownIsNotOverCounted() {
        var showCountHistogram =
                HistogramWatcher.newSingleRecordWatcher("Signin.SyncPromo.Shown.Count.Settings", 1);
        int promoShowCount =
                ChromeSharedPreferences.getInstance()
                        .readInt(
                                SyncPromoController.getPromoShowCountPreferenceName(
                                        SigninAccessPoint.SETTINGS));
        Assert.assertEquals(0, promoShowCount);
        Assert.assertEquals(
                0,
                ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT));
        startSettings();
        onViewWaiting(allOf(withId(R.id.signin_promo_view_container), isDisplayed()));

        promoShowCount =
                ChromeSharedPreferences.getInstance()
                        .readInt(
                                SyncPromoController.getPromoShowCountPreferenceName(
                                        SigninAccessPoint.SETTINGS));
        Assert.assertEquals(1, promoShowCount);
        Assert.assertEquals(
                1,
                ChromeSharedPreferences.getInstance()
                        .readInt(ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT));
        showCountHistogram.assertExpected();
    }

    @Test
    @SmallTest
    // Setting BrowserSignin suppresses the sync promo so the password settings preference
    // is visible without scrolling.
    @Policies.Add({
        @Policies.Item(key = "PasswordManagerEnabled", string = "false"),
        @Policies.Item(key = "BrowserSignin", string = "0")
    })
    public void testPasswordsItemClickableWhenManaged() {
        startSettings();
        onData(withKey(MainSettings.PREF_PASSWORDS))
                .inAdapterView(
                        allOf(
                                isDisplayed(),
                                hasDescendant(withText(R.string.password_manager_settings_title)),
                                hasDescendant(
                                        allOf(
                                                withText(R.string.managed_by_your_organization),
                                                isDisplayed()))));
        Assert.assertTrue(mMainSettings.findPreference(MainSettings.PREF_PASSWORDS).isEnabled());
        Assert.assertNotNull(
                mMainSettings
                        .findPreference(MainSettings.PREF_PASSWORDS)
                        .getOnPreferenceClickListener());
    }

    @Test
    @SmallTest
    @Policies.Remove({@Policies.Item(key = "PasswordManagerEnabled", string = "false")})
    // Setting BrowserSignin suppresses the sync promo so the password settings preference
    // is visible without scrolling.
    @Policies.Add(@Policies.Item(key = "BrowserSignin", string = "0"))
    public void testPasswordsItemEnabledWhenNotManaged() throws InterruptedException {
        startSettings();
        onData(withKey(MainSettings.PREF_PASSWORDS))
                .inAdapterView(
                        allOf(
                                isDisplayed(),
                                hasDescendant(withText(R.string.password_manager_settings_title)),
                                hasDescendant(
                                        allOf(
                                                withText(R.string.managed_by_your_organization),
                                                not(isDisplayed())))));
        Assert.assertTrue(mMainSettings.findPreference(MainSettings.PREF_PASSWORDS).isEnabled());
        Assert.assertNotNull(
                mMainSettings
                        .findPreference(MainSettings.PREF_PASSWORDS)
                        .getOnPreferenceClickListener());
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.PLUS_ADDRESSES_ENABLED)
    public void testPlusAddressesHiddenWhenNotEnabled() {
        Assert.assertFalse(ChromeFeatureList.isEnabled(ChromeFeatureList.PLUS_ADDRESSES_ENABLED));
        startSettings();
        Assert.assertNull(mMainSettings.findPreference(MainSettings.PREF_PLUS_ADDRESSES));
    }

    @Test
    @SmallTest
    public void testPlusAddressesHiddenWhenLabelIsEmpty() {
        Assert.assertTrue(
                ChromeFeatureList.getFieldTrialParamByFeature(
                                ChromeFeatureList.PLUS_ADDRESSES_ENABLED, "settings-label")
                        .isEmpty());
        startSettings();
        Assert.assertNull(mMainSettings.findPreference(MainSettings.PREF_PLUS_ADDRESSES));
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add({
        "enable-features=PlusAddressesEnabled:"
                + "settings-label/PlusAddressesTestTitle/"
                + "manage-url/https%3A%2F%2Ftest.plusaddresses.google.com"
    })
    public void testPlusAddressesEnabled() {
        startSettings();
        Preference preference = mMainSettings.findPreference(MainSettings.PREF_PLUS_ADDRESSES);
        Assert.assertNotNull(preference);
        Assert.assertTrue(preference.isVisible());
        Assert.assertEquals(preference.getTitle(), "PlusAddressesTestTitle");
        onView(withId(R.id.recycler_view))
                .perform(scrollTo(hasDescendant(withText("PlusAddressesTestTitle"))));
        onView(withText("PlusAddressesTestTitle")).perform(click());
        intended(IntentMatchers.hasData("https://test.plusaddresses.google.com"));
    }

    @Test
    @SmallTest
    public void testHomeModulesConfigSettingsWithCustomizableModule() {
        when(mHomeModulesConfigManager.hasModuleShownInSettings()).thenReturn(true);
        HomeModulesConfigManager.setInstanceForTesting(mHomeModulesConfigManager);
        startSettings();
        assertSettingsExists(
                MainSettings.PREF_HOME_MODULES_CONFIG, HomeModulesConfigSettings.class);
    }

    @Test
    @SmallTest
    public void testHomeModulesConfigSettingsWithoutCustomizableModule() {
        when(mHomeModulesConfigManager.hasModuleShownInSettings()).thenReturn(false);
        HomeModulesConfigManager.setInstanceForTesting(mHomeModulesConfigManager);
        startSettings();
        Assert.assertNull(
                "Home modules config setting should not be shown on automotive",
                mMainSettings.findPreference(MainSettings.PREF_HOME_MODULES_CONFIG));
    }

    @Test
    @SmallTest
    @EnableFeatures({
        ChromeFeatureList.TAB_GROUP_SYNC_ANDROID,
        ChromeFeatureList.TAB_GROUP_SYNC_AUTO_OPEN_KILL_SWITCH
    })
    @DisableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER)
    public void testTabsSettingsOn_GroupSync_KillSwitchInactive() {
        startSettings();
        assertSettingsExists(MainSettings.PREF_TABS, TabsSettings.class);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_SYNC_ANDROID)
    @DisableFeatures({
        ChromeFeatureList.ANDROID_TAB_DECLUTTER,
        ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID,
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_SYNC_AUTO_OPEN_KILL_SWITCH
    })
    public void testTabsSettingsOn_GroupSync_KillSwitchActive() {
        startSettings();
        Assert.assertNull(
                "Tabs settings should not be shown",
                mMainSettings.findPreference(MainSettings.PREF_TABS));
    }

    @Test
    @SmallTest
    @DisableFeatures({
        ChromeFeatureList.TAB_GROUP_SYNC_ANDROID,
        ChromeFeatureList.TAB_GROUP_SYNC_AUTO_OPEN_KILL_SWITCH
    })
    @EnableFeatures(ChromeFeatureList.ANDROID_TAB_DECLUTTER)
    public void testTabsSettingsOn_Declutter() {
        startSettings();
        assertSettingsExists(MainSettings.PREF_TABS, TabsSettings.class);
    }

    @Test
    @SmallTest
    @DisableFeatures({
        ChromeFeatureList.ANDROID_TAB_DECLUTTER,
        ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID,
        ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
        ChromeFeatureList.TAB_GROUP_SYNC_ANDROID
    })
    @EnableFeatures(ChromeFeatureList.TAB_GROUP_SYNC_AUTO_OPEN_KILL_SWITCH)
    public void testTabsSettingsOff() {
        startSettings();
        Assert.assertNull(
                "Tabs settings should not be shown",
                mMainSettings.findPreference(MainSettings.PREF_TABS));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testSafetyHubFlagOn() {
        startSettings();
        if (BuildInfo.getInstance().isAutomotive) {
            Assert.assertNull(
                    "Safety hub should not be shown on automotive",
                    mMainSettings.findPreference(MainSettings.PREF_SAFETY_HUB));
            Assert.assertNull(
                    "Safety check should not be shown on automotive",
                    mMainSettings.findPreference(MainSettings.PREF_SAFETY_CHECK));
            return;
        }

        assertSettingsExists(MainSettings.PREF_SAFETY_HUB, SafetyHubFragment.class);
        // Safety check should be hidden when safety hub is enabled.
        Assert.assertNull(
                "Safety check setting should be hidden",
                mMainSettings.findPreference(MainSettings.PREF_SAFETY_CHECK));

        // Verify that the correct metrics are logged.
        HistogramWatcher histogramExpectation =
                HistogramWatcher.newSingleRecordWatcher(
                        SafetyHubMetricUtils.EXTERNAL_INTERACTIONS_HISTOGRAM_NAME,
                        SafetyHubMetricUtils.ExternalInteractions.OPEN_FROM_SETTINGS_PAGE);
        onView(withId(R.id.recycler_view))
                .perform(scrollTo(hasDescendant(withText(R.string.prefs_safety_check))));
        onView(withText(R.string.prefs_safety_check)).perform(click());
        histogramExpectation.assertExpected();
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.SAFETY_HUB)
    public void testSafetyHubFlagOff() {
        startSettings();
        if (BuildInfo.getInstance().isAutomotive) {
            Assert.assertNull(
                    "Safety hub should not be shown on automotive",
                    mMainSettings.findPreference(MainSettings.PREF_SAFETY_HUB));
            Assert.assertNull(
                    "Safety check should not be shown on automotive",
                    mMainSettings.findPreference(MainSettings.PREF_SAFETY_CHECK));
        } else {
            assertSettingsExists(MainSettings.PREF_SAFETY_CHECK, SafetyCheckSettingsFragment.class);
            // Safety hub should be hidden when the flag is disabled.
            Assert.assertNull(
                    "Safety hub setting should be hidden",
                    mMainSettings.findPreference(MainSettings.PREF_SAFETY_HUB));
        }
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testAndroidAddressBarFlagOn() {
        startSettings();
        // This setting should only appear for certain devices, even if the flag is enabled. Since
        // this is an instrumentation test there's not a good way to fake or force device
        // characteristics, so we just fork the test's behavior based on the eligibility state.
        if (!ToolbarPositionController.isToolbarPositionCustomizationEnabled(
                mSettingsActivityTestRule.getActivity(), false)) {
            Assert.assertNull(
                    "Address Bar should not be shown for for ineligible devices",
                    mMainSettings.findPreference(MainSettings.PREF_ADDRESS_BAR));
        } else {
            assertSettingsExists(MainSettings.PREF_ADDRESS_BAR, AddressBarSettingsFragment.class);
        }
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testAndroidAddressBarFlagOff() {
        startSettings();
        Assert.assertNull(
                "Address Bar should not be shown when flag is off, regardless of device",
                mMainSettings.findPreference(MainSettings.PREF_ADDRESS_BAR));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.DEFAULT_BROWSER_PROMO_ANDROID2)
    public void testDefaultBrowserPromoCard() throws InterruptedException {
        when(mTestTracker.shouldTriggerHelpUI(any())).thenReturn(true);
        TrackerFactory.setTrackerForTests(mTestTracker);
        when(mMockDefaultBrowserPromoUtils.shouldShowNonRoleManagerPromo(any())).thenReturn(true);
        DefaultBrowserPromoUtils.setInstanceForTesting(mMockDefaultBrowserPromoUtils);

        startSettings();
        Preference preference = mMainSettings.findPreference(MainSettings.PREF_SETTINGS_PROMO_CARD);
        Assert.assertNotNull(
                "Settings promo preference exist when feature flag is enabled", preference);
        Assert.assertTrue("Settings promo card is not showing", preference.isVisible());
    }

    private void startSettings() {
        mSettingsActivityTestRule.startSettingsActivity();
        mMainSettings = mSettingsActivityTestRule.getFragment();
        Assert.assertNotNull("SettingsActivity failed to launch.", mMainSettings);
    }

    private void configureMockSearchEngine() {
        TemplateUrlServiceFactory.setInstanceForTesting(mMockTemplateUrlService);
        Mockito.doReturn(mMockSearchEngine)
                .when(mMockTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();

        Mockito.doReturn(SEARCH_ENGINE_SHORT_NAME).when(mMockSearchEngine).getShortName();
    }

    private void waitForOptionsMenu() {
        CriteriaHelper.pollUiThread(
                () -> {
                    return mSettingsActivityTestRule
                                    .getActivity()
                                    .findViewById(R.id.menu_id_general_help)
                            != null;
                });
    }

    /**
     * Assert the target preference exists in the main settings and creates the expected fragment,
     * then return that preference.
     *
     * @param prefKey preference key for {@link
     *     androidx.preference.PreferenceFragmentCompat#findPreference(CharSequence)}
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
            Assert.assertEquals(
                    "Preference class is different.",
                    settingsFragmentClass,
                    Class.forName(pref.getFragment()));
        } catch (ClassNotFoundException e) {
            throw new AssertionError("Pref fragment <" + pref.getFragment() + "> is not found.");
        }
        return pref;
    }

    private boolean supportNotificationSettings() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return false;
        return PackageManagerUtils.canResolveActivity(
                new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS));
    }

    private boolean supportThirdPartyFillingSetting() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.O;
    }
}
