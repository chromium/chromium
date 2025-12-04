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
import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.settings.MainSettings.PREF_APPEARANCE;
import static org.chromium.chrome.browser.settings.MainSettings.PREF_TOOLBAR_SHORTCUT;
import static org.chromium.chrome.browser.settings.MainSettings.PREF_UI_THEME;

import android.app.Activity;
import android.content.Intent;
import android.os.Looper;
import android.provider.Settings;
import android.text.TextUtils;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
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
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
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
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.about_settings.AboutChromeSettings;
import org.chromium.chrome.browser.appearance.settings.AppearanceSettingsFragment;
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
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridge;
import org.chromium.chrome.browser.password_manager.PasswordManagerUtilBridgeJni;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.privacy.settings.PrivacySettings;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safety_hub.SafetyHubFragment;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.settings.SearchEngineSettings;
import org.chromium.chrome.browser.signin.SigninAndHistorySyncActivityLauncherImpl;
import org.chromium.chrome.browser.sync.FakeSyncServiceImpl;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.SyncTestRule;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SignInPreference;
import org.chromium.chrome.browser.tasks.tab_management.TabsSettings;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController;
import org.chromium.chrome.browser.toolbar.adaptive.settings.AdaptiveToolbarSettingsFragment;
import org.chromium.chrome.browser.toolbar.settings.AddressBarSettingsFragment;
import org.chromium.chrome.browser.tracing.settings.DeveloperSettings;
import org.chromium.chrome.browser.ui.default_browser_promo.DefaultBrowserPromoUtils;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.NoAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.BottomSheetSigninAndHistorySyncConfig.WithAccountSigninMode;
import org.chromium.chrome.browser.ui.signin.SigninAndHistorySyncActivityLauncher;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncConfig;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettings;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;
import org.chromium.components.signin.test.util.AccountCapabilitiesBuilder;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.GmsCoreVersionRestriction;
import org.chromium.ui.test.util.ViewUtils;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.text.SpanApplier.SpanInfo;

import java.io.IOException;
import java.util.stream.Stream;

/** Test for {@link MainSettings}. Main purpose is to have a quick confidence check on the xml. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "show-autofill-signatures"})
@DoNotBatch(reason = "Tests cannot run batched because they launch a Settings activity.")
@DisableFeatures({ChromeFeatureList.DATA_SHARING, ChromeFeatureList.SETTINGS_MULTI_COLUMN})
public class MainSettingsFragmentTest {
    private static final String SEARCH_ENGINE_SHORT_NAME = "Google";

    private static final int RENDER_TEST_REVISION = 14;
    private static final String RENDER_TEST_DESCRIPTION =
            "Alert icon on identity error for signed in users";

    private final HomepageTestRule mHomepageTestRule = new HomepageTestRule();

    private final SyncTestRule mSyncTestRule = new SyncTestRule();

    private final SettingsActivityTestRule<MainSettings> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(MainSettings.class);

    // SettingsActivity needs to be initialized and destroyed with the mock
    // signin environment setup in SyncTestRule
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

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

    @Mock public TemplateUrlService mMockTemplateUrlService;
    @Mock public TemplateUrl mMockSearchEngine;

    @Mock private PasswordManagerUtilBridge.Natives mPasswordManagerUtilBridgeJniMock;

    @Mock private SigninAndHistorySyncActivityLauncher mSigninAndHistorySyncActivityLauncher;
    @Mock private HomeModulesConfigManager mHomeModulesConfigManager;

    @Mock private Tracker mTestTracker;
    @Mock private DefaultBrowserPromoUtils mMockDefaultBrowserPromoUtils;

    private MainSettings mMainSettings;

    @Before
    public void setup() {
        // ObservableSupplierImpl needs a Looper.
        Looper.prepare();
        InstrumentationRegistry.getInstrumentation().setInTouchMode(true);
        PasswordManagerUtilBridgeJni.setInstanceForTesting(mPasswordManagerUtilBridgeJniMock);
        SigninAndHistorySyncActivityLauncherImpl.setLauncherForTest(
                mSigninAndHistorySyncActivityLauncher);
        DeveloperSettings.setIsEnabledForTests(true);
        Intents.init();

        // Keep render tests consistent by suppressing "new" labels.
        final var prefs = ChromeSharedPreferences.getInstance();
        Stream.of(
                        ChromePreferenceKeys.ADDRESS_BAR_SETTINGS_VIEW_COUNT,
                        ChromePreferenceKeys.APPEARANCE_SETTINGS_VIEW_COUNT)
                .forEach(key -> prefs.writeInt(key, MainSettings.NEW_LABEL_MAX_VIEW_COUNT));
    }

    @After
    public void tearDown() {
        Intents.release();
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    // TODO(crbug.com/433576895): Re-enable containment and multi-column feature
    // once the test is fixed.
    @DisableFeatures({
        ChromeFeatureList.ANDROID_SETTINGS_CONTAINMENT,
        ChromeFeatureList.SETTINGS_MULTI_COLUMN
    })
    public void testRenderSignedOutAccountManagementRows() throws IOException {
        startSettings();
        waitForOptionsMenu();

        View accountRow =
                mSettingsActivityTestRule
                        .getActivity()
                        .findViewById(R.id.account_management_account_row);
        ChromeRenderTestRule.sanitize(accountRow);
        mRenderTestRule.render(accountRow, "main_settings_signed_out_account");
        View googleServicesRow =
                mSettingsActivityTestRule
                        .getActivity()
                        .findViewById(R.id.account_management_google_services_row);
        ChromeRenderTestRule.sanitize(googleServicesRow);
        mRenderTestRule.render(googleServicesRow, "main_settings_signed_out_google_services");
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @Policies.Add({@Policies.Item(key = "BrowserSignin", string = "0")})
    // TODO(crbug.com/433576895): Re-enable containment and multi-column feature
    // once the test is fixed.
    @DisableFeatures({
        ChromeFeatureList.ANDROID_SETTINGS_CONTAINMENT,
        ChromeFeatureList.SETTINGS_MULTI_COLUMN
    })
    public void testRenderSigninDisabledByPolicyAccountRow() throws IOException {
        startSettings();
        waitForOptionsMenu();

        View accountRow =
                mSettingsActivityTestRule
                        .getActivity()
                        .findViewById(R.id.account_management_account_row);
        ChromeRenderTestRule.sanitize(accountRow);
        mRenderTestRule.render(accountRow, "main_settings_signin_disabled_by_policy_account");
    }

    /** Test for the "Account" section. */
    @Test
    @SmallTest
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
        assertSettingsExists(MainSettings.PREF_AUTOFILL_OPTIONS, null);
        assertSettingsExists(MainSettings.PREF_AUTOFILL_SECTION, null);
        assertSettingsExists(MainSettings.PREF_PASSWORDS, null);
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

        // Verification for summary for the search engine and the homepage
        Assert.assertEquals(
                "Homepage summary is different than homepage state",
                mMainSettings.getString(R.string.text_on),
                mMainSettings.findPreference(MainSettings.PREF_HOMEPAGE).getSummary().toString());

        // Assert for advanced section
        assertSettingsExists("privacy", PrivacySettings.class);
        if (DeviceInfo.isAutomotive()) {
            Assert.assertNull(
                    "Safety check should not be shown on automotive",
                    mMainSettings.findPreference(MainSettings.PREF_SAFETY_HUB));
        } else {
            assertSettingsExists(MainSettings.PREF_SAFETY_HUB, SafetyHubFragment.class);
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
    @LargeTest
    @Feature({"Sync"})
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
    @Policies.Add(@Policies.Item(key = "SyncDisabled", string = "true"))
    public void testPressingSignOutSyncDisabled() {
        CoreAccountInfo accountInfo = mSyncTestRule.setUpAccountAndSignInWithoutWaitingForTesting();

        startSettings();

        onView(withText(accountInfo.getEmail())).perform(click());
        onView(withId(R.id.recycler_view)).perform(RecyclerViewActions.scrollToLastPosition());
        onView(withText(R.string.sign_out)).perform(click());
        Assert.assertNull(mSyncTestRule.getSigninTestRule().getPrimaryAccount(ConsentLevel.SIGNIN));

        Activity activity = mSettingsActivityTestRule.getActivity();
        final String expectedSnackbarMessage =
                activity.getString(
                        R.string.account_settings_sign_out_snackbar_message_sync_disabled);
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
    @MediumTest
    public void testSignInRowLaunchesSignInFlowForSignedOutAccounts() {
        mSyncTestRule.addTestAccount();
        startSettings();

        onView(withId(R.id.recycler_view))
                .perform(scrollTo(hasDescendant(withText(R.string.signin_settings_title))));
        onView(withText(R.string.signin_settings_subtitle)).check(matches(isDisplayed()));
        onView(withText(R.string.signin_settings_title)).perform(click());

        ArgumentCaptor<BottomSheetSigninAndHistorySyncConfig> configCaptor =
                ArgumentCaptor.forClass(BottomSheetSigninAndHistorySyncConfig.class);
        verify(mSigninAndHistorySyncActivityLauncher)
                .createBottomSheetSigninIntentOrShowError(
                        any(Activity.class),
                        any(Profile.class),
                        configCaptor.capture(),
                        eq(SigninAccessPoint.SETTINGS));
        BottomSheetSigninAndHistorySyncConfig config = configCaptor.getValue();
        assertEquals(NoAccountSigninMode.BOTTOM_SHEET, config.noAccountSigninMode);
        assertEquals(
                WithAccountSigninMode.DEFAULT_ACCOUNT_BOTTOM_SHEET, config.withAccountSigninMode);
        assertEquals(HistorySyncConfig.OptInMode.OPTIONAL, config.historyOptInMode);
        assertNull(config.selectedCoreAccountId);
    }

    // Tests that no alert icon is visible if there are no identity errors.
    @Test
    @SmallTest
    // Specifies the test to run only with the GMS Core version greater than or equal to 24w15 which
    // is the min version that supports split stores UPM backend, to avoid
    // UserActionableError.NEEDS_UPM_BACKEND_UPGRADE.
    @Restriction(GmsCoreVersionRestriction.RESTRICTION_TYPE_VERSION_GE_24W15)
    public void testSigninRowShowsNoAlertWhenNoIdentityErrors() {
        // Sign-in and open settings.
        mSyncTestRule.setUpAccountAndSignInForTesting();
        startSettings();

        assertSettingsExists(MainSettings.PREF_SIGN_IN, ManageSyncSettings.class);
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

        assertSettingsExists(MainSettings.PREF_SIGN_IN, ManageSyncSettings.class);
        onView(allOf(withId(R.id.alert_icon), isDisplayed())).check(matches(isDisplayed()));
    }

    @Test
    @LargeTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    public void testRenderOnIdentityErrorForSignedInUsers() throws IOException {
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

        // Wait for the default browser promo view to disappear to avoid flakiness due to race
        // conditions.
        ViewUtils.waitForViewCheckingState(withId(R.id.promo_card_view), ViewUtils.VIEW_NULL);
        View view =
                mSettingsActivityTestRule
                        .getActivity()
                        .findViewById(android.R.id.content)
                        .getRootView();
        ChromeRenderTestRule.sanitize(view);
        mRenderTestRule.render(view, "main_settings_signed_in_identity_error");
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
    public void testManageSyncRowIsNotShownWithoutSyncConsent() throws InterruptedException {
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
    public void
            testAccountManagementRowForChildAccountWithNonDisplayableAccountEmailWithEmptyDisplayName()
                    throws InterruptedException {
        startSettings();

        // Account set up.
        // If both fullName and givenName are empty, accountCapabilities is ignored.
        final SigninTestRule signinTestRule = mSyncTestRule.getSigninTestRule();
        AccountInfo accountInfo = TestAccounts.TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL_AND_NO_NAME;
        signinTestRule.addAccount(accountInfo);
        // Child accounts are signed-in automatically in the background.
        signinTestRule.waitForSignin(accountInfo);

        SignInPreference signInPreference = mMainSettings.findPreference(MainSettings.PREF_SIGN_IN);
        CriteriaHelper.pollUiThread(
                () -> {
                    return !signInPreference
                            .getProfileDataCache()
                            .getProfileDataOrDefault(
                                    TestAccounts.TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL_AND_NO_NAME
                                            .getEmail())
                            .hasDisplayableEmailAddress();
                });
        ThreadUtils.runOnUiThreadBlocking(signInPreference::syncStateChanged);

        mSettingsActivityTestRule.startSettingsActivity();

        onView(withText(TestAccounts.TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL_AND_NO_NAME.getEmail()))
                .check(doesNotExist());
        onView(allOf(withText(R.string.default_google_account_username), isDisplayed()))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    public void testRemoveSettings() {
        DeveloperSettings.setIsEnabledForTests(false);
        startSettings();
        Assert.assertNull(
                "Preference should be disabled: " + MainSettings.PREF_DEVELOPER,
                mMainSettings.findPreference(MainSettings.PREF_DEVELOPER));
    }

    @Test
    @SmallTest
    // Setting BrowserSignin suppresses the sync promo so the password settings preference
    // is visible without scrolling.
    @Policies.Add({
        @Policies.Item(key = "PasswordManagerEnabled", string = "false"),
        @Policies.Item(key = "BrowserSignin", string = "0")
    })
    @DisabledTest(message = "Proabably never worked. crbug.com/446200399")
    public void testPasswordsItemClickableWhenManaged() {
        startSettings();
        var managedStrMatcher =
                allOf(withText(R.string.managed_by_your_organization), isDisplayed());
        onData(withKey(MainSettings.PREF_PASSWORDS))
                .inAdapterView(
                        allOf(
                                hasDescendant(withText(R.string.password_manager_settings_title)),
                                hasDescendant(managedStrMatcher)))
                .check(matches(isDisplayed()));

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
    @DisabledTest(message = "Proabably never worked. crbug.com/446200399")
    public void testPasswordsItemEnabledWhenNotManaged() throws InterruptedException {
        startSettings();
        var managedStrMatcher =
                allOf(withText(R.string.managed_by_your_organization), not(isDisplayed()));
        onData(withKey(MainSettings.PREF_PASSWORDS))
                .inAdapterView(
                        allOf(
                                hasDescendant(withText(R.string.password_manager_settings_title)),
                                hasDescendant(managedStrMatcher)))
                .check(matches(isDisplayed()));
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
        Assert.assertEquals("PlusAddressesTestTitle", preference.getTitle());
        onView(withId(R.id.recycler_view))
                .perform(scrollTo(hasDescendant(withText("PlusAddressesTestTitle"))));
        onView(withText("PlusAddressesTestTitle")).perform(click());
        intended(IntentMatchers.hasData("https://test.plusaddresses.google.com"));
    }

    /**
     * Verifies that when the feature flag is enabled, the PREF_HOME_MODULES_CONFIG is removed from
     * the settings page.
     */
    // TODO(crbug.com/376238770): Remove @EnableFeatures once the feature flag is turned on by
    // default.
    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.NEW_TAB_PAGE_CUSTOMIZATION)
    public void testHomeModulesConfigSettingsWithCustomizableModuleWhileFeatureTurnOn() {
        when(mHomeModulesConfigManager.hasModuleShownInSettings()).thenReturn(true);
        HomeModulesConfigManager.setInstanceForTesting(mHomeModulesConfigManager);
        startSettings();
        assertSettingsNotExists(MainSettings.PREF_HOME_MODULES_CONFIG);
    }

    /**
     * Verifies that when the feature flag is turned off, the PREF_HOME_MODULES_CONFIG is removed
     * from the settings page, only if hasModuleShownInSettings returns false.
     */
    // TODO(crbug.com/376238770): Removes this test when the feature flag is turned on by default.
    @Test
    @SmallTest
    @DisableFeatures("NewTabPageCustomization")
    public void testHomeModulesConfigSettingsWithCustomizableModuleWhileFeatureTurnOff() {
        when(mHomeModulesConfigManager.hasModuleShownInSettings()).thenReturn(true);
        HomeModulesConfigManager.setInstanceForTesting(mHomeModulesConfigManager);
        startSettings();
        assertSettingsExists(
                MainSettings.PREF_HOME_MODULES_CONFIG, HomeModulesConfigSettings.class);

        when(mHomeModulesConfigManager.hasModuleShownInSettings()).thenReturn(false);
        HomeModulesConfigManager.setInstanceForTesting(mHomeModulesConfigManager);
        startSettings();
        assertSettingsNotExists(MainSettings.PREF_HOME_MODULES_CONFIG);
    }

    @Test
    @SmallTest
    public void testTabsSettingsOn() {
        startSettings();
        assertSettingsExists(MainSettings.PREF_TABS, TabsSettings.class);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testAndroidAddressBarFlagOn() {
        startSettings();
        // This setting should only appear for certain devices, even if the flag is enabled. Since
        // this is an instrumentation test there's not a good way to fake or force device
        // characteristics, so we just fork the test's behavior based on the eligibility state.
        boolean showSetting =
                !DeviceInfo.isAutomotive()
                        && (DeviceInfo.isFoldable()
                                || !DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                                        mSettingsActivityTestRule.getActivity()));
        if (!showSetting) {
            Assert.assertNull(
                    "Address Bar should not be shown for for ineligible devices",
                    mMainSettings.findPreference(MainSettings.PREF_ADDRESS_BAR));
        } else {
            assertSettingsExists(MainSettings.PREF_ADDRESS_BAR, AddressBarSettingsFragment.class);
            Assert.assertEquals(
                    mMainSettings.getString(R.string.address_bar_settings),
                    mMainSettings
                            .findPreference(MainSettings.PREF_ADDRESS_BAR)
                            .getTitle()
                            .toString());
        }
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testAndroidAddressBar_newLabel() {
        Assume.assumeThat(supportAddressBarSettings(), is(true));
        testNewPreferenceLabel(
                AddressBarSettingsFragment.class,
                MainSettings.PREF_ADDRESS_BAR,
                ChromePreferenceKeys.ADDRESS_BAR_SETTINGS_VIEW_COUNT,
                R.string.address_bar_settings);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_BOTTOM_TOOLBAR)
    public void testAndroidAddressBar_cleanUpBadPrefValue() {
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.ADDRESS_BAR_SETTINGS_CLICKED, 1);
        startSettings();

        if (!supportAddressBarSettings()) {
            return;
        }

        assertSettingsExists(MainSettings.PREF_ADDRESS_BAR, AddressBarSettingsFragment.class);
        Assert.assertEquals(
                mMainSettings.getString(R.string.address_bar_settings),
                mMainSettings.findPreference(MainSettings.PREF_ADDRESS_BAR).getTitle().toString());
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
        when(mTestTracker.shouldTriggerHelpUi(any())).thenReturn(true);
        TrackerFactory.setTrackerForTests(mTestTracker);
        when(mMockDefaultBrowserPromoUtils.shouldShowNonRoleManagerPromo(any())).thenReturn(true);
        DefaultBrowserPromoUtils.setInstanceForTesting(mMockDefaultBrowserPromoUtils);

        startSettings();
        Preference preference = mMainSettings.findPreference(MainSettings.PREF_SETTINGS_PROMO_CARD);
        Assert.assertNotNull(
                "Settings promo preference exist when feature flag is enabled", preference);
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_APPEARANCE_SETTINGS)
    public void testAppearanceSettingsEnabled() {
        startSettings();
        assertSettingsExists(PREF_APPEARANCE, AppearanceSettingsFragment.class);
        Assert.assertNull(mMainSettings.findPreference(PREF_TOOLBAR_SHORTCUT));
        Assert.assertNull(mMainSettings.findPreference(PREF_UI_THEME));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.ANDROID_APPEARANCE_SETTINGS)
    public void testAppearanceSettingsDisabled() {
        startSettings();
        Assert.assertNull(mMainSettings.findPreference(PREF_APPEARANCE));
        assertSettingsExists(PREF_TOOLBAR_SHORTCUT, AdaptiveToolbarSettingsFragment.class);
        final var themePref = assertSettingsExists(PREF_UI_THEME, ThemeSettingsFragment.class);
        Assert.assertEquals(
                ThemeSettingsEntry.SETTINGS,
                themePref.getExtras().getInt(ThemeSettingsFragment.KEY_THEME_SETTINGS_ENTRY));
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ANDROID_APPEARANCE_SETTINGS)
    public void testAppearanceSettingsNewLabel() {
        testNewPreferenceLabel(
                AppearanceSettingsFragment.class,
                MainSettings.PREF_APPEARANCE,
                ChromePreferenceKeys.APPEARANCE_SETTINGS_VIEW_COUNT,
                R.string.appearance_settings);
    }

    private void startSettings() {
        mSettingsActivityTestRule.startSettingsActivity();
        mMainSettings = mSettingsActivityTestRule.getFragment();
        Assert.assertNotNull("SettingsActivity failed to launch.", mMainSettings);
    }

    private void restartSettings() {
        mSettingsActivityTestRule.finishActivity();
        startSettings();
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

    private void assertSettingsNotExists(String prefKey) {
        Preference pref = mMainSettings.findPreference(prefKey);
        Assert.assertNull(pref);
    }

    private boolean supportAddressBarSettings() {
        return ToolbarPositionController.isToolbarPositionCustomizationEnabled(
                ContextUtils.getApplicationContext(), false);
    }

    private boolean supportNotificationSettings() {
        return PackageManagerUtils.canResolveActivity(
                new Intent(Settings.ACTION_APP_NOTIFICATION_SETTINGS));
    }

    private void testNewPreferenceLabel(
            @NonNull Class prefFragmentClass,
            @NonNull String prefKey,
            @NonNull String viewCountPrefKey,
            @StringRes int titleId) {
        // Set up.
        final var prefs = ChromeSharedPreferences.getInstance();
        prefs.writeInt(viewCountPrefKey, MainSettings.NEW_LABEL_MAX_VIEW_COUNT);
        startSettings();

        final String prefTitleWithoutNewLabel = mMainSettings.getString(titleId);

        // Case: Pref has been viewed `NEW_LABEL_MAX_VIEW_COUNT` times.
        assertSettingsExists(prefKey, prefFragmentClass);
        Assert.assertEquals(
                prefTitleWithoutNewLabel,
                mMainSettings.findPreference(prefKey).getTitle().toString());

        final String prefTitleWithNewLabel =
                SpanApplier.applySpans(
                                mMainSettings.getString(
                                        R.string.prefs_new_label, prefTitleWithoutNewLabel),
                                new SpanInfo("<new>", "</new>"))
                        .toString();

        // Case: Pref has been viewed fewer than `NEW_LABEL_MAX_VIEW_COUNT` times.
        prefs.writeInt(viewCountPrefKey, MainSettings.NEW_LABEL_MAX_VIEW_COUNT - 1);
        restartSettings();
        Assert.assertEquals(
                prefTitleWithNewLabel, mMainSettings.findPreference(prefKey).getTitle().toString());

        // Case: Pref has been clicked.
        mMainSettings.findPreference(prefKey).performClick();
        restartSettings();
        Assert.assertEquals(
                prefTitleWithoutNewLabel,
                mMainSettings.findPreference(prefKey).getTitle().toString());
    }
}
