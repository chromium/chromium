// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.os.Bundle;

import androidx.preference.Preference;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.browser_ui.settings.FragmentSettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Fragment containing Safety hub. */
public class SafetyHubFragment extends SafetyHubBaseFragment
        implements FragmentSettingsLauncher,
                UnusedSitePermissionsBridge.Observer,
                NotificationPermissionReviewBridge.Observer {
    private static final String PREF_PASSWORDS = "passwords_account";
    private static final String PREF_UPDATE = "update_check";
    private static final String PREF_UNUSED_PERMISSIONS = "permissions";
    private static final String PREF_NOTIFICATIONS_REVIEW = "notifications_review";

    private SafetyHubModuleDelegate mDelegate;
    private SettingsLauncher mSettingsLauncher;
    private UnusedSitePermissionsBridge mUnusedSitePermissionsBridge;
    private PropertyModel mPermissionsModel;
    private NotificationPermissionReviewBridge mNotificationPermissionReviewBridge;
    private PropertyModel mNotificationsModel;

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.safety_hub_preferences);
        getActivity().setTitle(R.string.prefs_safety_check);

        // Set up account-level password check preference.
        Preference passwordCheckPreference = findPreference(PREF_PASSWORDS);
        int compromisedPasswordsCount =
                UserPrefs.get(getProfile()).getInteger(Pref.BREACHED_CREDENTIALS_COUNT);

        PropertyModel passwordCheckPropertyModel =
                new PropertyModel.Builder(
                                SafetyHubModuleProperties.PASSWORD_CHECK_SAFETY_HUB_MODULE_KEYS)
                        .with(
                                SafetyHubModuleProperties.IS_VISIBLE,
                                mDelegate.shouldShowPasswordCheckModule())
                        .with(
                                SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT,
                                compromisedPasswordsCount)
                        .with(
                                SafetyHubModuleProperties.ON_CLICK_LISTENER,
                                () -> mDelegate.showPasswordCheckUI(getContext()))
                        .build();

        PropertyModelChangeProcessor.create(
                passwordCheckPropertyModel,
                passwordCheckPreference,
                SafetyHubModuleViewBinder::bindPasswordCheckProperties);

        // Set up update check preference.
        Preference updateCheckPreference = findPreference(PREF_UPDATE);

        PropertyModel updateCheckPropertyModel =
                new PropertyModel.Builder(
                                SafetyHubModuleProperties.UPDATE_CHECK_SAFETY_HUB_MODULE_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .with(SafetyHubModuleProperties.UPDATE_STATUS, mDelegate.getUpdateStatus())
                        .build();

        PropertyModelChangeProcessor.create(
                updateCheckPropertyModel,
                updateCheckPreference,
                SafetyHubModuleViewBinder::bindUpdateCheckProperties);

        // Set up permissions preference.
        Preference permissionsPreference = findPreference(PREF_UNUSED_PERMISSIONS);

        mPermissionsModel =
                new PropertyModel.Builder(SafetyHubModuleProperties.PERMISSIONS_MODULE_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .with(
                                SafetyHubModuleProperties.ON_CLICK_LISTENER,
                                () ->
                                        mSettingsLauncher.launchSettingsActivity(
                                                getContext(), SafetyHubPermissionsFragment.class))
                        .build();

        PropertyModelChangeProcessor.create(
                mPermissionsModel,
                permissionsPreference,
                SafetyHubModuleViewBinder::bindPermissionsProperties);

        mUnusedSitePermissionsBridge = UnusedSitePermissionsBridge.getForProfile(getProfile());
        mUnusedSitePermissionsBridge.addObserver(this);

        // Set up notifications review preference.
        Preference notificationsPreference = findPreference(PREF_NOTIFICATIONS_REVIEW);

        mNotificationsModel =
                new PropertyModel.Builder(
                                SafetyHubModuleProperties.NOTIFICATIONS_REVIEW_MODULE_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .with(
                                SafetyHubModuleProperties.ON_CLICK_LISTENER,
                                () ->
                                        mSettingsLauncher.launchSettingsActivity(
                                                getContext(), SafetyHubNotificationsFragment.class))
                        .build();

        PropertyModelChangeProcessor.create(
                mNotificationsModel,
                notificationsPreference,
                SafetyHubModuleViewBinder::bindNotificationsReviewProperties);

        mNotificationPermissionReviewBridge =
                NotificationPermissionReviewBridge.getForProfile(getProfile());
        mNotificationPermissionReviewBridge.addObserver(this);
    }

    @Override
    public void onResume() {
        super.onResume();

        updatePermissionsPreference();
        updateNotificationsReviewPreference();
    }

    @Override
    public void revokedPermissionsChanged() {
        updatePermissionsPreference();
    }

    @Override
    public void notificationPermissionsChanged() {
        updateNotificationsReviewPreference();
    }

    @Override
    public void setSettingsLauncher(SettingsLauncher settingsLauncher) {
        mSettingsLauncher = settingsLauncher;
    }

    public void setDelegate(SafetyHubModuleDelegate safetyHubModuleDelegate) {
        mDelegate = safetyHubModuleDelegate;
    }

    private void updatePermissionsPreference() {
        int sitesWithUnusedPermissionsCount =
                mUnusedSitePermissionsBridge.getRevokedPermissions().length;
        mPermissionsModel.set(
                SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT,
                sitesWithUnusedPermissionsCount);
    }

    private void updateNotificationsReviewPreference() {
        int notificationPermissionsForReviewCount =
                mNotificationPermissionReviewBridge.getNotificationPermissions().size();
        mNotificationsModel.set(
                SafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
                notificationPermissionsForReviewCount);
    }
}
