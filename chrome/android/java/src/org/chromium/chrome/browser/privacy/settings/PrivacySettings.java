// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import static org.chromium.components.content_settings.PrefNames.COOKIE_CONTROLS_MODE;

import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.style.ClickableSpan;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthSettingSwitchPreference;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsFragment;
import org.chromium.chrome.browser.privacy.secure_dns.SecureDnsSettings;
import org.chromium.chrome.browser.privacy_guide.PrivacyGuideInteractions;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxReferrer;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxSettingsBaseFragment;
import org.chromium.chrome.browser.safe_browsing.AdvancedProtectionStatusManagerAndroidBridge;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.safety_hub.SafetyHubExpandablePreference;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleProperties;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleViewBinder;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ssl.HttpsFirstModeSettingsFragment;
import org.chromium.chrome.browser.sync.settings.GoogleServicesSettings;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.usage_stats.UsageStatsConsentDialog;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.ContentSettingsResources;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.util.TraceEventVectorDrawableCompat;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionProvider;
import org.chromium.components.permissions.OsAdditionalSecurityPermissionUtil;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.concurrent.TimeUnit;
import java.util.function.Consumer;

/** Fragment to keep track of the all the privacy related preferences. */
public class PrivacySettings extends ChromeBaseSettingsFragment
        implements Preference.OnPreferenceChangeListener {
    private static final String PREF_CAN_MAKE_PAYMENT = "can_make_payment";
    private static final String PREF_PRELOAD_PAGES = "preload_pages";
    private static final String PREF_HTTPS_FIRST_MODE = "https_first_mode";
    // TODO(crbug.com/349860796): Remove once new settings are fully rolled out.
    private static final String PREF_HTTPS_FIRST_MODE_LEGACY = "https_first_mode_legacy";
    private static final String PREF_SECURE_DNS = "secure_dns";
    private static final String PREF_USAGE_STATS = "usage_stats_reporting";
    private static final String PREF_SAFE_BROWSING = "safe_browsing";
    private static final String PREF_PASSWORD_LEAK_DETECTION = "password_leak_detection";
    private static final String PREF_SYNC_AND_SERVICES_LINK = "sync_and_services_link";
    private static final String PREF_PRIVACY_SANDBOX = "privacy_sandbox";
    private static final String PREF_PRIVACY_GUIDE = "privacy_guide";
    private static final String PREF_INCOGNITO_LOCK = "incognito_lock";
    private static final String PREF_JAVASCRIPT_OPTIMIZER = "javascript_optimizer";
    private static final String PREF_INCOGNITO_TRACKING_PROTECTIONS =
            "incognito_tracking_protections";
    @VisibleForTesting static final String PREF_DO_NOT_TRACK = "do_not_track";
    @VisibleForTesting static final String PREF_THIRD_PARTY_COOKIES = "third_party_cookies";
    @VisibleForTesting static final String PREF_TRACKING_PROTECTION = "tracking_protection";
    private static final String PREF_ADVANCED_PROTECTION_INFO = "advanced_protection_info";

    private IncognitoLockSettings mIncognitoLockSettings;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    /** Called when the advanced-protection javascript-optimizer-settings link is clicked. */
    @VisibleForTesting
    public static void onJavascriptOptimizerLinkClicked(Context context) {
        Bundle extras = new Bundle();
        extras.putString(SingleCategorySettings.EXTRA_CATEGORY, "javascript_optimizer");
        SettingsNavigation navigation = SettingsNavigationFactory.createSettingsNavigation();
        navigation.startSettings(context, SingleCategorySettings.class, extras);
    }

    /** Creates {@link SpanInfo} for link which has the passed-in tag. */
    private static SpanApplier.SpanInfo createLink(
            Context context, String tag, @Nullable Consumer<Context> clickCallback) {
        String startTag = "<" + tag + ">";
        String endTag = "</" + tag + ">";
        Callback<View> onClickCallback =
                v -> {
                    clickCallback.accept(context);
                };
        return new SpanApplier.SpanInfo(
                startTag, endTag, new ChromeClickableSpan(context, onClickCallback));
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mPageTitle.set(getString(R.string.prefs_privacy_security));

        SettingsUtils.addPreferencesFromResource(this, R.xml.privacy_preferences);

        Preference incognitoTrackingProtectionsPreference =
                findPreference(PREF_INCOGNITO_TRACKING_PROTECTIONS);
        incognitoTrackingProtectionsPreference.setVisible(
                shouldShowIncognitoTrackingProtectionsUi());

        Preference sandboxPreference = findPreference(PREF_PRIVACY_SANDBOX);
        // Overwrite the click listener to pass a correct referrer to the fragment.
        sandboxPreference.setOnPreferenceClickListener(
                preference -> {
                    PrivacySandboxSettingsBaseFragment.launchPrivacySandboxSettings(
                            getContext(), PrivacySandboxReferrer.PRIVACY_SETTINGS);
                    return true;
                });

        PrivacySandboxBridge privacySandboxBridge = new PrivacySandboxBridge(getProfile());
        if (privacySandboxBridge.isPrivacySandboxRestricted()) {
            if (privacySandboxBridge.isRestrictedNoticeEnabled()) {
                // Update the summary to one that describes only ad measurement if ad-measurement
                // is available to restricted users.
                sandboxPreference.setSummary(
                        getContext()
                                .getString(
                                        R.string
                                                .settings_ad_privacy_restricted_link_row_sub_label));
            } else {
                // Hide the Privacy Sandbox if it is restricted and ad-measurement is not
                // available to restricted users.
                getPreferenceScreen().removePreference(sandboxPreference);
            }
        }

        Preference privacyGuidePreference = findPreference(PREF_PRIVACY_GUIDE);
        // Record the launch of PG from the S&P link-row entry point
        privacyGuidePreference.setOnPreferenceClickListener(
                preference -> {
                    RecordUserAction.record("Settings.PrivacyGuide.StartPrivacySettings");
                    RecordHistogram.recordEnumeratedHistogram(
                            "Settings.PrivacyGuide.EntryExit",
                            PrivacyGuideInteractions.SETTINGS_LINK_ROW_ENTRY,
                            PrivacyGuideInteractions.MAX_VALUE);
                    UserPrefs.get(getProfile()).setBoolean(Pref.PRIVACY_GUIDE_VIEWED, true);
                    return false;
                });
        if (getProfile().isChild()
                || ManagedBrowserUtils.isBrowserManaged(getProfile())
                || ManagedBrowserUtils.isProfileManaged(getProfile())) {
            getPreferenceScreen().removePreference(privacyGuidePreference);
        }

        IncognitoReauthSettingSwitchPreference incognitoReauthPreference =
                (IncognitoReauthSettingSwitchPreference) findPreference(PREF_INCOGNITO_LOCK);
        mIncognitoLockSettings = new IncognitoLockSettings(incognitoReauthPreference, getProfile());
        mIncognitoLockSettings.setUpIncognitoReauthPreference(getActivity());

        maybeShowAdvancedProtectionSection();

        Preference safeBrowsingPreference = findPreference(PREF_SAFE_BROWSING);
        safeBrowsingPreference.setSummary(
                SafeBrowsingSettingsFragment.getSafeBrowsingSummaryString(
                        getContext(), getProfile()));
        safeBrowsingPreference.setOnPreferenceClickListener(
                (preference) -> {
                    preference
                            .getExtras()
                            .putInt(
                                    SafeBrowsingSettingsFragment.ACCESS_POINT,
                                    SettingsAccessPoint.PARENT_SETTINGS);
                    return false;
                });

        setHasOptionsMenu(true);

        ChromeSwitchPreference passwordLeakTogglePref =
                (ChromeSwitchPreference) findPreference(PREF_PASSWORD_LEAK_DETECTION);
        passwordLeakTogglePref.setOnPreferenceChangeListener(this);

        ChromeSwitchPreference canMakePaymentPref =
                (ChromeSwitchPreference) findPreference(PREF_CAN_MAKE_PAYMENT);
        canMakePaymentPref.setOnPreferenceChangeListener(this);

        // TODO(crbug.com/349860796): Remove old version (PREF_HTTPS_FIRST_MODE_LEGACY)
        // when new settings are fully rolled out.
        Preference httpsFirstModePref = findPreference(PREF_HTTPS_FIRST_MODE);
        ChromeSwitchPreference httpsFirstModeLegacySwitchPref =
                (ChromeSwitchPreference) findPreference(PREF_HTTPS_FIRST_MODE_LEGACY);
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.HTTPS_FIRST_BALANCED_MODE)) {
            // Hide the old toggle pref if the feature flag is enabled.
            httpsFirstModeLegacySwitchPref.setVisible(false);

            httpsFirstModePref.setSummary(
                    HttpsFirstModeSettingsFragment.getSummary(getContext(), getProfile()));
        } else {
            // Hide the new pref item if the feature flag isn't enabled.
            httpsFirstModePref.setVisible(false);

            httpsFirstModeLegacySwitchPref.setOnPreferenceChangeListener(this);
            httpsFirstModeLegacySwitchPref.setManagedPreferenceDelegate(
                    new ChromeManagedPreferenceDelegate(getProfile()) {
                        @Override
                        public boolean isPreferenceControlledByPolicy(Preference preference) {
                            String key = preference.getKey();
                            assert PREF_HTTPS_FIRST_MODE_LEGACY.equals(key)
                                    : "Unexpected preference key: " + key;
                            return UserPrefs.get(getProfile())
                                    .isManagedPreference(Pref.HTTPS_ONLY_MODE_ENABLED);
                        }

                        @Override
                        public boolean isPreferenceClickDisabled(Preference preference) {
                            // Advanced Protection automatically enables HTTPS-Only Mode so
                            // lock the setting.
                            return isPreferenceControlledByPolicy(preference)
                                    || AdvancedProtectionStatusManagerAndroidBridge
                                            .isUnderAdvancedProtection();
                        }
                    });
            httpsFirstModeLegacySwitchPref.setChecked(
                    UserPrefs.get(getProfile()).getBoolean(Pref.HTTPS_ONLY_MODE_ENABLED));
            if (AdvancedProtectionStatusManagerAndroidBridge.isUnderAdvancedProtection()) {
                httpsFirstModeLegacySwitchPref.setSummary(
                        getContext()
                                .getString(
                                        R.string
                                                .settings_https_first_mode_with_advanced_protection_summary));
            }
        }

        Preference syncAndServicesLink = findPreference(PREF_SYNC_AND_SERVICES_LINK);
        syncAndServicesLink.setSummary(buildFooterString());

        Preference thirdPartyCookies = findPreference(PREF_THIRD_PARTY_COOKIES);
        if (showTrackingProtectionUi()) {
            if (thirdPartyCookies != null) thirdPartyCookies.setVisible(false);
            Preference trackingProtection = findPreference(PREF_TRACKING_PROTECTION);
            trackingProtection.setVisible(true);
        } else if (thirdPartyCookies != null) {
            thirdPartyCookies
                    .getExtras()
                    .putString(SingleCategorySettings.EXTRA_CATEGORY, thirdPartyCookies.getKey());
            thirdPartyCookies
                    .getExtras()
                    .putString(
                            SingleCategorySettings.EXTRA_TITLE,
                            thirdPartyCookies.getTitle().toString());
        }

        Preference javascriptOptimizerPref = findPreference(PREF_JAVASCRIPT_OPTIMIZER);
        javascriptOptimizerPref
                .getExtras()
                .putString(SingleCategorySettings.EXTRA_CATEGORY, javascriptOptimizerPref.getKey());
        javascriptOptimizerPref
                .getExtras()
                .putString(
                        SingleCategorySettings.EXTRA_TITLE,
                        javascriptOptimizerPref.getTitle().toString());

        Bundle arguments = getArguments();
        if (arguments != null
                && arguments
                        .keySet()
                        .contains(
                                PrivacySettingsNavigation
                                        .EXTRA_FOCUS_ADVANCED_PROTECTION_SECTION)) {
            scrollToPreference(PREF_ADVANCED_PROTECTION_INFO);
        }

        updatePreferences();
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    private SpannableString buildFooterString() {
        ClickableSpan servicesLink =
                new ClickableSpan() {
                    @Override
                    public void onClick(View view) {
                        SettingsNavigationFactory.createSettingsNavigation()
                                .startSettings(getActivity(), GoogleServicesSettings.class);
                    }
                };

        ClickableSpan accountSettingsLink =
                new ClickableSpan() {
                    @Override
                    public void onClick(View view) {
                        SettingsNavigationFactory.createSettingsNavigation()
                                .startSettings(
                                        getActivity(),
                                        ManageSyncSettings.class,
                                        ManageSyncSettings.createArguments(false));
                    }
                };
        if (IdentityServicesProvider.get()
                        .getIdentityManager(getProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN)
                == null) {
            // User is signed out, show the string with one link to "Google Services".
            return SpanApplier.applySpans(
                    getString(R.string.privacy_chrome_data_and_google_services_signed_out_footer),
                    new SpanApplier.SpanInfo("<link>", "</link>", servicesLink));
        }
        // Otherwise, show the string with both links to account settings and "Google Services".
        return SpanApplier.applySpans(
                getString(R.string.privacy_chrome_data_and_google_services_footer),
                new SpanApplier.SpanInfo("<link1>", "</link1>", accountSettingsLink),
                new SpanApplier.SpanInfo("<link2>", "</link2>", servicesLink));
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (PREF_CAN_MAKE_PAYMENT.equals(key)) {
            UserPrefs.get(getProfile())
                    .setBoolean(Pref.CAN_MAKE_PAYMENT_ENABLED, (boolean) newValue);
        } else if (PREF_HTTPS_FIRST_MODE_LEGACY.equals(key)) {
            // TODO(crbug.com/349860796): Remove once new settings are fully rolled out.
            UserPrefs.get(getProfile())
                    .setBoolean(Pref.HTTPS_ONLY_MODE_ENABLED, (boolean) newValue);
        } else if (PREF_PASSWORD_LEAK_DETECTION.equals(key)) {
            UserPrefs.get(getProfile())
                    .setBoolean(Pref.PASSWORD_LEAK_DETECTION_ENABLED, (boolean) newValue);
        }
        return true;
    }

    @Override
    public void onStart() {
        super.onStart();
        updatePreferences();
    }

    /** Updates the preferences. */
    public void updatePreferences() {
        ChromeSwitchPreference passwordLeakTogglePref =
                (ChromeSwitchPreference) findPreference(PREF_PASSWORD_LEAK_DETECTION);
        if (passwordLeakTogglePref != null) {
            passwordLeakTogglePref.setEnabled(
                    !UserPrefs.get(getProfile())
                            .isManagedPreference(Pref.PASSWORD_LEAK_DETECTION_ENABLED));
            passwordLeakTogglePref.setChecked(
                    UserPrefs.get(getProfile()).getBoolean(Pref.PASSWORD_LEAK_DETECTION_ENABLED));
        }

        ChromeSwitchPreference canMakePaymentPref =
                (ChromeSwitchPreference) findPreference(PREF_CAN_MAKE_PAYMENT);
        if (canMakePaymentPref != null) {
            canMakePaymentPref.setChecked(
                    UserPrefs.get(getProfile()).getBoolean(Pref.CAN_MAKE_PAYMENT_ENABLED));
        }

        Preference doNotTrackPref = findPreference(PREF_DO_NOT_TRACK);
        if (doNotTrackPref != null) {
            doNotTrackPref.setSummary(
                    UserPrefs.get(getProfile()).getBoolean(Pref.ENABLE_DO_NOT_TRACK)
                            ? R.string.text_on
                            : R.string.text_off);
        }

        Preference preloadPagesPreference = findPreference(PREF_PRELOAD_PAGES);
        if (preloadPagesPreference != null) {
            preloadPagesPreference.setSummary(
                    PreloadPagesSettingsFragment.getPreloadPagesSummaryString(
                            getContext(), getProfile()));
        }

        Preference secureDnsPref = findPreference(PREF_SECURE_DNS);
        if (secureDnsPref != null && secureDnsPref.isVisible()) {
            secureDnsPref.setSummary(SecureDnsSettings.getSummary(getContext()));
        }

        Preference safeBrowsingPreference = findPreference(PREF_SAFE_BROWSING);
        if (safeBrowsingPreference != null && safeBrowsingPreference.isVisible()) {
            safeBrowsingPreference.setSummary(
                    SafeBrowsingSettingsFragment.getSafeBrowsingSummaryString(
                            getContext(), getProfile()));
        }

        Preference httpsFirstModePreference = findPreference(PREF_HTTPS_FIRST_MODE);
        if (httpsFirstModePreference != null && httpsFirstModePreference.isVisible()) {
            httpsFirstModePreference.setSummary(
                    HttpsFirstModeSettingsFragment.getSummary(getContext(), getProfile()));
        }

        Preference usageStatsPref = findPreference(PREF_USAGE_STATS);
        if (usageStatsPref != null) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
                    && UserPrefs.get(getProfile()).getBoolean(Pref.USAGE_STATS_ENABLED)) {
                usageStatsPref.setOnPreferenceClickListener(
                        preference -> {
                            UsageStatsConsentDialog.create(
                                            getActivity(),
                                            getProfile(),
                                            true,
                                            (didConfirm) -> {
                                                if (didConfirm) {
                                                    updatePreferences();
                                                }
                                            })
                                    .show();
                            return true;
                        });
            } else {
                getPreferenceScreen().removePreference(usageStatsPref);
            }
        }

        mIncognitoLockSettings.updateIncognitoReauthPreferenceIfNeeded(getActivity());

        Preference trackingProtection = findPreference(PREF_TRACKING_PROTECTION);
        if (trackingProtection != null) {
            trackingProtection.setSummary(
                    ContentSettingsResources.getTrackingProtectionListSummary(
                            UserPrefs.get(getProfile())
                                    .getBoolean(Pref.BLOCK_ALL3PC_TOGGLE_ENABLED)));
        }

        Preference thirdPartyCookies = findPreference(PREF_THIRD_PARTY_COOKIES);
        if (thirdPartyCookies != null) {
            @CookieControlsMode
            int cookieControlsMode = UserPrefs.get(getProfile()).getInteger(COOKIE_CONTROLS_MODE);
            thirdPartyCookies.setSummary(
                    ChromeFeatureList.isEnabled(ChromeFeatureList.ALWAYS_BLOCK_3PCS_INCOGNITO)
                                    && cookieControlsMode == CookieControlsMode.INCOGNITO_ONLY
                            ? R.string.third_party_cookies_link_row_sub_label_enabled
                            : ContentSettingsResources.getThirdPartyCookieListSummary(
                                    UserPrefs.get(getProfile()).getInteger(COOKIE_CONTROLS_MODE)));
        }

        Preference javascriptOptimizerPref = findPreference(PREF_JAVASCRIPT_OPTIMIZER);
        javascriptOptimizerPref.setSummary(
                WebsitePreferenceBridge.isCategoryEnabled(
                                getProfile(), ContentSettingsType.JAVASCRIPT_OPTIMIZER)
                        ? R.string.website_settings_category_javascript_optimizer_allowed_list
                        : R.string.website_settings_category_javascript_optimizer_blocked_list);
    }

    private boolean showTrackingProtectionUi() {
        return UserPrefs.get(getProfile()).getBoolean(Pref.TRACKING_PROTECTION3PCD_ENABLED)
                || ChromeFeatureList.isEnabled(ChromeFeatureList.TRACKING_PROTECTION_3PCD);
    }

    private boolean shouldShowIncognitoTrackingProtectionsUi() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.FINGERPRINTING_PROTECTION_UX)
                || ChromeFeatureList.isEnabled(ChromeFeatureList.IP_PROTECTION_UX);
    }

    /** Shows the advanced-protection-section if needed. */
    private void maybeShowAdvancedProtectionSection() {
        Context context = getContext();
        SafetyHubExpandablePreference advancedProtectionInfoPreference =
                (SafetyHubExpandablePreference) findPreference(PREF_ADVANCED_PROTECTION_INFO);

        @Nullable OsAdditionalSecurityPermissionProvider additionalSecurityProvider =
                OsAdditionalSecurityPermissionUtil.getProviderInstance();
        if (!shouldShowAdvancedProtectionInfo() || additionalSecurityProvider == null) {
            advancedProtectionInfoPreference.setVisible(false);
            return;
        }

        @Nullable Drawable additionalSecurityIcon =
                additionalSecurityProvider.getColorfulAdvancedProtectionIcon(getContext());

        Consumer<Context> androidAdvancedProtectionLinkAction =
                (linkContext) -> {
                    Intent intent =
                            additionalSecurityProvider.getIntentForOsAdvancedProtectionSettings();
                    if (intent != null) {
                        IntentUtils.safeStartActivity(linkContext, intent);
                    }
                };
        Consumer<Context> javascriptOptimizerLinkAction =
                (linkContext) -> {
                    PrivacySettings.onJavascriptOptimizerLinkClicked(linkContext);
                };
        SpanApplier.SpanInfo[] spans =
                new SpanApplier.SpanInfo[] {
                    createLink(
                            context,
                            "link_android_advanced_protection",
                            androidAdvancedProtectionLinkAction),
                    createLink(context, "link_javascript_optimizer", javascriptOptimizerLinkAction)
                };
        String advancedProtectionSectionMessageTemplate =
                getString(
                        R.string.settings_privacy_and_security_advanced_protection_section_message);
        SpannableString span =
                SpanApplier.applySpans(advancedProtectionSectionMessageTemplate, spans);

        PropertyModel advancedProtectionInfoModel =
                new PropertyModel.Builder(SafetyHubModuleProperties.ALL_KEYS)
                        .with(SafetyHubModuleProperties.ICON, additionalSecurityIcon)
                        .with(SafetyHubModuleProperties.SUMMARY, span)
                        .build();
        PropertyModelChangeProcessor.create(
                advancedProtectionInfoModel,
                advancedProtectionInfoPreference,
                SafetyHubModuleViewBinder::bindProperties);
    }

    /** Returns whether the advanced-protection section should be shown. */
    private boolean shouldShowAdvancedProtectionInfo() {
        if (!AdvancedProtectionStatusManagerAndroidBridge.isUnderAdvancedProtection()) {
            return false;
        }
        long updateTimeMs =
                ChromeSharedPreferences.getInstance()
                        .readLong(
                                ChromePreferenceKeys.OS_ADVANCED_PROTECTION_SETTING_UPDATED_TIME,
                                0);
        return updateTimeMs == 0
                || ((System.currentTimeMillis() - updateTimeMs) < TimeUnit.DAYS.toMillis(90));
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(
                TraceEventVectorDrawableCompat.create(
                        getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            getHelpAndFeedbackLauncher()
                    .show(getActivity(), getString(R.string.help_context_privacy), null);
            return true;
        }
        return false;
    }

    @Override
    public void onDestroy() {
        if (mIncognitoLockSettings != null) {
            mIncognitoLockSettings.destroy();
        }
        super.onDestroy();
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }
}
