// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Preferences for features related to price tracking. */
public class PriceNotificationSettingsFragment extends ChromeBaseSettingsFragment {
    @VisibleForTesting static final String PREF_MOBILE_NOTIFICATIONS = "mobile_notifications_text";

    @VisibleForTesting static final String PREF_EMAIL_NOTIFICATIONS = "send_email_switch";

    private final PrefChangeRegistrar mPrefChangeRegistrar = new PrefChangeRegistrar();

    private PrefService mPrefService;
    private TextMessagePreference mMobileNotificationsText;
    private ChromeSwitchPreference mEmailNotificationsSwitch;
    private NotificationManagerProxy mNotificationManagerProxy;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mPrefService = UserPrefs.get(getProfile());
        mNotificationManagerProxy =
                new NotificationManagerProxyImpl(ContextUtils.getApplicationContext());

        SettingsUtils.addPreferencesFromResource(this, R.xml.price_notification_preferences);
        mPageTitle.set(getString(R.string.price_notifications_settings_detailed_page_title));

        mMobileNotificationsText =
                (TextMessagePreference) findPreference(PREF_MOBILE_NOTIFICATIONS);
        updateMobileNotificationsText();

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            // App settings are only available on O+.
            getPreferenceScreen().removePreference(mMobileNotificationsText);
            mMobileNotificationsText = null;
        }

        mEmailNotificationsSwitch =
                (ChromeSwitchPreference) findPreference(PREF_EMAIL_NOTIFICATIONS);
        mEmailNotificationsSwitch.setOnPreferenceChangeListener(this::onPreferenceChange);
        CoreAccountInfo info =
                IdentityServicesProvider.get()
                        .getIdentityManager(getProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        if (info != null) {
            String email = info.getEmail();
            mEmailNotificationsSwitch.setSummary(
                    getString(R.string.price_notifications_settings_email_description, email));
            mPrefChangeRegistrar.addObserver(
                    Pref.PRICE_EMAIL_NOTIFICATIONS_ENABLED, this::updateEmailNotificationSwitch);
            updateEmailNotificationSwitch();
        } else {
            mEmailNotificationsSwitch.setVisible(false);
        }
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onStart() {
        super.onStart();
        updateMobileNotificationsText();

        ShoppingServiceFactory.getForProfile(getProfile()).fetchPriceEmailPref();
    }

    /** Handle preference changes from any of the toggles in this UI. */
    private boolean onPreferenceChange(Preference preference, Object newValue) {
        if (PREF_EMAIL_NOTIFICATIONS.equals(preference.getKey())) {
            mPrefService.setBoolean(Pref.PRICE_EMAIL_NOTIFICATIONS_ENABLED, (boolean) newValue);
            return true;
        }
        return false;
    }

    /** Update the appearance of the email notification switch to reflect its corresponding pref. */
    private void updateEmailNotificationSwitch() {
        boolean emailEnabled = mPrefService.getBoolean(Pref.PRICE_EMAIL_NOTIFICATIONS_ENABLED);
        if (mEmailNotificationsSwitch.isChecked() != emailEnabled) {
            mEmailNotificationsSwitch.setChecked(emailEnabled);
        }
    }

    /**
     * Update the text displayed in the mobile notifications section. The text changes based on the
     * state of notifications and the price tracking channel
     */
    private void updateMobileNotificationsText() {
        if (mMobileNotificationsText == null) return;

        String linkText = getString(R.string.chrome_notification_settings_for_price_tracking);

        String settingsFullText;
        if (arePriceTrackingNotificationsEnabled()) {
            settingsFullText =
                    getString(
                            R.string.price_notifications_settings_mobile_description_on, linkText);
        } else {
            settingsFullText =
                    getString(
                            R.string.price_notifications_settings_mobile_description_off, linkText);
        }

        SpanApplier.SpanInfo info =
                new SpanApplier.SpanInfo(
                        "<link>",
                        "</link>",
                        new NoUnderlineClickableSpan(getContext(), (view) -> launchAppSettings()));
        SpanApplier.applySpans(settingsFullText, info);

        mMobileNotificationsText.setSummary(SpanApplier.applySpans(settingsFullText, info));
    }

    /** @return True if both app-level and price tracking notifications are enabled. */
    private boolean arePriceTrackingNotificationsEnabled() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel =
                    mNotificationManagerProxy.getNotificationChannel(
                            ChromeChannelDefinitions.ChannelId.PRICE_DROP_DEFAULT);
            if (mNotificationManagerProxy.areNotificationsEnabled()
                    && channel != null
                    && channel.getImportance() != NotificationManager.IMPORTANCE_NONE) {
                return true;
            }
        }
        return false;
    }

    /** Launch app settings so the user can view or change notification settings. */
    private void launchAppSettings() {
        Intent notificationsPrefIntent = new Intent();
        notificationsPrefIntent.setAction(Settings.ACTION_APP_NOTIFICATION_SETTINGS);
        notificationsPrefIntent.putExtra(
                Settings.EXTRA_APP_PACKAGE, ContextUtils.getApplicationContext().getPackageName());
        PackageManager pm = getActivity().getPackageManager();
        if (notificationsPrefIntent.resolveActivity(pm) != null) {
            startActivity(notificationsPrefIntent);
        }
    }

    void setPrefServiceForTesting(PrefService prefs) {
        mPrefService = prefs;
    }
}
