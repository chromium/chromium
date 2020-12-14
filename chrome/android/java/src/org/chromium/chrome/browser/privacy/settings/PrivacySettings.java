// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import android.os.Build;
import android.os.Bundle;
import android.text.SpannableString;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy.secure_dns.SecureDnsSettings;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.settings.GoogleServicesSettings;
import org.chromium.chrome.browser.sync.settings.ManageSyncSettings;
import org.chromium.chrome.browser.sync.settings.SyncAndServicesSettings;
import org.chromium.chrome.browser.usage_stats.UsageStatsConsentDialog;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * Fragment to keep track of the all the privacy related preferences.
 */
public class PrivacySettings
        extends PreferenceFragmentCompat implements Preference.OnPreferenceChangeListener {
    private static final String PREF_CAN_MAKE_PAYMENT = "can_make_payment";
    private static final String PREF_NETWORK_PREDICTIONS = "preload_pages";
    private static final String PREF_SECURE_DNS = "secure_dns";
    private static final String PREF_USAGE_STATS = "usage_stats_reporting";
    private static final String PREF_DO_NOT_TRACK = "do_not_track";
    private static final String PREF_SAFE_BROWSING = "safe_browsing";
    private static final String PREF_SYNC_AND_SERVICES_LINK = "sync_and_services_link";
    private static final String PREF_CLEAR_BROWSING_DATA = "clear_browsing_data";
    private static final String PREF_PRIVACY_SANDBOX = "privacy_sandbox";
    private static final String[] NEW_PRIVACY_PREFERENCE_ORDER = {PREF_CLEAR_BROWSING_DATA,
            PREF_SAFE_BROWSING, PREF_CAN_MAKE_PAYMENT, PREF_NETWORK_PREDICTIONS, PREF_USAGE_STATS,
            PREF_SECURE_DNS, PREF_DO_NOT_TRACK, PREF_PRIVACY_SANDBOX, PREF_SYNC_AND_SERVICES_LINK};

    private ManagedPreferenceDelegate mManagedPreferenceDelegate;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        PrivacyPreferencesManagerImpl privacyPrefManager =
                PrivacyPreferencesManagerImpl.getInstance();
        SettingsUtils.addPreferencesFromResource(this, R.xml.privacy_preferences);
        assert NEW_PRIVACY_PREFERENCE_ORDER.length
                == getPreferenceScreen().getPreferenceCount()
            : "All preferences in the screen should be added in the new order list. "
                        + "If you add a new preference, please also update "
                        + "NEW_PRIVACY_PREFERENCE_ORDER.";

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_REORDERED_ANDROID)) {
            for (int i = 0; i < NEW_PRIVACY_PREFERENCE_ORDER.length; i++) {
                findPreference(NEW_PRIVACY_PREFERENCE_ORDER[i]).setOrder(i);
            }
        }

        // Remove Privacy Sandbox settings if the corresponding flag is disabled.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.PRIVACY_SANDBOX_SETTINGS)) {
            getPreferenceScreen().removePreference(findPreference(PREF_PRIVACY_SANDBOX));
        }

        // If the flag for adding a "Safe Browsing" section UI is enabled, a "Safe Browsing" section
        // will be added under this section and this section will be renamed to "Privacy and
        // security". See (go/esb-clank-dd) for more context.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.SAFE_BROWSING_SECTION_UI)) {
            getActivity().setTitle(R.string.prefs_privacy_security);
            Preference safeBrowsingPreference = findPreference(PREF_SAFE_BROWSING);
            safeBrowsingPreference.setSummary(
                    SafeBrowsingSettingsFragment.getSafeBrowsingSummaryString(getContext()));
            safeBrowsingPreference.setOnPreferenceClickListener((preference) -> {
                preference.getExtras().putInt(SafeBrowsingSettingsFragment.ACCESS_POINT,
                        SettingsAccessPoint.PARENT_SETTINGS);
                return false;
            });
        } else {
            getActivity().setTitle(R.string.prefs_privacy);
            getPreferenceScreen().removePreference(findPreference(PREF_SAFE_BROWSING));
        }
        setHasOptionsMenu(true);

        mManagedPreferenceDelegate = createManagedPreferenceDelegate();

        ChromeSwitchPreference canMakePaymentPref =
                (ChromeSwitchPreference) findPreference(PREF_CAN_MAKE_PAYMENT);
        canMakePaymentPref.setOnPreferenceChangeListener(this);

        ChromeSwitchPreference networkPredictionPref =
                (ChromeSwitchPreference) findPreference(PREF_NETWORK_PREDICTIONS);
        networkPredictionPref.setChecked(
                PrivacyPreferencesManagerImpl.getInstance().getNetworkPredictionEnabled());
        networkPredictionPref.setOnPreferenceChangeListener(this);
        networkPredictionPref.setManagedPreferenceDelegate(mManagedPreferenceDelegate);

        Preference secureDnsPref = findPreference(PREF_SECURE_DNS);
        secureDnsPref.setVisible(SecureDnsSettings.isUiEnabled());

        Preference syncAndServicesLink = findPreference(PREF_SYNC_AND_SERVICES_LINK);
        syncAndServicesLink.setSummary(buildSyncAndServicesLink());

        updateSummaries();
    }

    private SpannableString buildSyncAndServicesLink() {
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.MOBILE_IDENTITY_CONSISTENCY)) {
            NoUnderlineClickableSpan syncAndServicesLink =
                    new NoUnderlineClickableSpan(getResources(), v -> {
                        settingsLauncher.launchSettingsActivity(getActivity(),
                                SyncAndServicesSettings.class,
                                SyncAndServicesSettings.createArguments(false));
                    });
            return SpanApplier.applySpans(getString(R.string.privacy_sync_and_services_link_legacy),
                    new SpanApplier.SpanInfo("<link>", "</link>", syncAndServicesLink));
        }

        NoUnderlineClickableSpan servicesLink = new NoUnderlineClickableSpan(getResources(), v -> {
            settingsLauncher.launchSettingsActivity(getActivity(), GoogleServicesSettings.class);
        });
        if (IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SYNC)
                == null) {
            // Sync is off, show the string with one link to "Google Services".
            return SpanApplier.applySpans(
                    getString(R.string.privacy_sync_and_services_link_sync_off),
                    new SpanApplier.SpanInfo("<link>", "</link>", servicesLink));
        }
        // Otherwise, show the string with both links to "Sync" and "Google Services".
        NoUnderlineClickableSpan syncLink = new NoUnderlineClickableSpan(getResources(), v -> {
            settingsLauncher.launchSettingsActivity(getActivity(), ManageSyncSettings.class,
                    ManageSyncSettings.createArguments(false));
        });
        return SpanApplier.applySpans(getString(R.string.privacy_sync_and_services_link_sync_on),
                new SpanApplier.SpanInfo("<link1>", "</link1>", syncLink),
                new SpanApplier.SpanInfo("<link2>", "</link2>", servicesLink));
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        if (PREF_CAN_MAKE_PAYMENT.equals(key)) {
            UserPrefs.get(Profile.getLastUsedRegularProfile())
                    .setBoolean(Pref.CAN_MAKE_PAYMENT_ENABLED, (boolean) newValue);
        } else if (PREF_NETWORK_PREDICTIONS.equals(key)) {
            PrivacyPreferencesManagerImpl.getInstance().setNetworkPredictionEnabled(
                    (boolean) newValue);
        }

        return true;
    }

    @Override
    public void onResume() {
        super.onResume();
        updateSummaries();
    }

    /**
     * Updates the summaries for several preferences.
     */
    public void updateSummaries() {
        PrefService prefService = UserPrefs.get(Profile.getLastUsedRegularProfile());

        ChromeSwitchPreference canMakePaymentPref =
                (ChromeSwitchPreference) findPreference(PREF_CAN_MAKE_PAYMENT);
        if (canMakePaymentPref != null) {
            canMakePaymentPref.setChecked(prefService.getBoolean(Pref.CAN_MAKE_PAYMENT_ENABLED));
        }

        Preference doNotTrackPref = findPreference(PREF_DO_NOT_TRACK);
        if (doNotTrackPref != null) {
            doNotTrackPref.setSummary(prefService.getBoolean(Pref.ENABLE_DO_NOT_TRACK)
                            ? R.string.text_on
                            : R.string.text_off);
        }

        Preference secureDnsPref = findPreference(PREF_SECURE_DNS);
        if (secureDnsPref != null && secureDnsPref.isVisible()) {
            secureDnsPref.setSummary(SecureDnsSettings.getSummary(getContext()));
        }

        Preference safeBrowsingPreference = findPreference(PREF_SAFE_BROWSING);
        if (safeBrowsingPreference != null && safeBrowsingPreference.isVisible()) {
            safeBrowsingPreference.setSummary(
                    SafeBrowsingSettingsFragment.getSafeBrowsingSummaryString(getContext()));
        }

        Preference usageStatsPref = findPreference(PREF_USAGE_STATS);
        if (usageStatsPref != null) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
                    && prefService.getBoolean(Pref.USAGE_STATS_ENABLED)) {
                usageStatsPref.setOnPreferenceClickListener(preference -> {
                    UsageStatsConsentDialog
                            .create(getActivity(), true,
                                    (didConfirm) -> {
                                        if (didConfirm) {
                                            updateSummaries();
                                        }
                                    })
                            .show();
                    return true;
                });
            } else {
                getPreferenceScreen().removePreference(usageStatsPref);
            }
        }
    }

    private ChromeManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return preference -> {
            String key = preference.getKey();
            if (PREF_NETWORK_PREDICTIONS.equals(key)) {
                return PrivacyPreferencesManagerImpl.getInstance().isNetworkPredictionManaged();
            }
            return false;
        };
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(VectorDrawableCompat.create(
                getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            HelpAndFeedbackLauncherImpl.getInstance().show(getActivity(),
                    getString(R.string.help_context_privacy), Profile.getLastUsedRegularProfile(),
                    null);
            return true;
        }
        return false;
    }
}
