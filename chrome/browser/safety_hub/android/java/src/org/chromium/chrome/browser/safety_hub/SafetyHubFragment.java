// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.os.Bundle;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Fragment containing Safety hub. */
public class SafetyHubFragment extends SafetyHubBaseFragment
        implements UnusedSitePermissionsBridge.Observer,
                NotificationPermissionReviewBridge.Observer {
    private static final String PREF_PASSWORDS = "passwords_account";
    private static final String PREF_UPDATE = "update_check";
    private static final String PREF_UNUSED_PERMISSIONS = "permissions";
    private static final String PREF_NOTIFICATIONS_REVIEW = "notifications_review";
    private static final String PREF_SAFE_BROWSING = "safe_browsing";

    private SafetyHubModuleDelegate mDelegate;
    private UnusedSitePermissionsBridge mUnusedSitePermissionsBridge;
    private PropertyModel mPermissionsModel;
    private NotificationPermissionReviewBridge mNotificationPermissionReviewBridge;
    private PropertyModel mNotificationsModel;
    private PropertyModel mSafeBrowsingPropertyModel;

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.safety_hub_preferences);
        getActivity().setTitle(R.string.prefs_safety_check);
        setUpAccountPasswordCheckModule();
        setUpUpdateCheckModule();
        setUpPermissionsRevocationModule();
        setUpNotificationsReviewModule();
        setUpSafeBrowsingModule();
    }

    private void setUpAccountPasswordCheckModule() {
        SafetyHubExpandablePreference passwordCheckPreference = findPreference(PREF_PASSWORDS);
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
                                SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER,
                                v -> mDelegate.showPasswordCheckUI(getContext()))
                        .with(
                                SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER,
                                v -> mDelegate.showPasswordCheckUI(getContext()))
                        .build();

        PropertyModelChangeProcessor.create(
                passwordCheckPropertyModel,
                passwordCheckPreference,
                SafetyHubModuleViewBinder::bindPasswordCheckProperties);
    }

    private void setUpUpdateCheckModule() {
        SafetyHubExpandablePreference updateCheckPreference = findPreference(PREF_UPDATE);

        PropertyModel updateCheckPropertyModel =
                new PropertyModel.Builder(
                                SafetyHubModuleProperties.UPDATE_CHECK_SAFETY_HUB_MODULE_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .with(SafetyHubModuleProperties.UPDATE_STATUS, mDelegate.getUpdateStatus())
                        .with(
                                SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER,
                                v -> mDelegate.openGooglePlayStore(getContext()))
                        .with(
                                SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER,
                                v -> mDelegate.openGooglePlayStore(getContext()))
                        .build();

        PropertyModelChangeProcessor.create(
                updateCheckPropertyModel,
                updateCheckPreference,
                SafetyHubModuleViewBinder::bindUpdateCheckProperties);
    }

    private void setUpPermissionsRevocationModule() {
        SafetyHubExpandablePreference permissionsPreference =
                findPreference(PREF_UNUSED_PERMISSIONS);

        mPermissionsModel =
                new PropertyModel.Builder(SafetyHubModuleProperties.PERMISSIONS_MODULE_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .with(
                                SafetyHubModuleProperties.SECONDARY_BUTTON_LISTENER,
                                v -> launchSettingsActivity(SafetyHubPermissionsFragment.class))
                        .build();

        PropertyModelChangeProcessor.create(
                mPermissionsModel,
                permissionsPreference,
                SafetyHubModuleViewBinder::bindPermissionsProperties);

        mUnusedSitePermissionsBridge = UnusedSitePermissionsBridge.getForProfile(getProfile());
        mUnusedSitePermissionsBridge.addObserver(this);
    }

    private void setUpNotificationsReviewModule() {
        SafetyHubExpandablePreference notificationsPreference =
                findPreference(PREF_NOTIFICATIONS_REVIEW);

        mNotificationsModel =
                new PropertyModel.Builder(
                                SafetyHubModuleProperties.NOTIFICATIONS_REVIEW_MODULE_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .with(
                                SafetyHubModuleProperties.SECONDARY_BUTTON_LISTENER,
                                v -> launchSettingsActivity(SafetyHubNotificationsFragment.class))
                        .build();

        PropertyModelChangeProcessor.create(
                mNotificationsModel,
                notificationsPreference,
                SafetyHubModuleViewBinder::bindNotificationsReviewProperties);

        mNotificationPermissionReviewBridge =
                NotificationPermissionReviewBridge.getForProfile(getProfile());
        mNotificationPermissionReviewBridge.addObserver(this);
    }

    private void setUpSafeBrowsingModule() {
        SafetyHubExpandablePreference safeBrowsingPreference = findPreference(PREF_SAFE_BROWSING);

        mSafeBrowsingPropertyModel =
                new PropertyModel.Builder(SafetyHubModuleProperties.SAFE_BROWSING_MODULE_KEYS)
                        .with(SafetyHubModuleProperties.IS_VISIBLE, true)
                        .with(
                                SafetyHubModuleProperties.SECONDARY_BUTTON_LISTENER,
                                v -> launchSettingsActivity(SafeBrowsingSettingsFragment.class))
                        .with(
                                SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER,
                                v -> launchSettingsActivity(SafeBrowsingSettingsFragment.class))
                        .build();

        PropertyModelChangeProcessor.create(
                mSafeBrowsingPropertyModel,
                safeBrowsingPreference,
                SafetyHubModuleViewBinder::bindSafeBrowsingProperties);
    }

    @Override
    public void onResume() {
        super.onResume();

        updatePermissionsPreference();
        updateNotificationsReviewPreference();
        updateSafeBrowsingPreference();
    }

    @Override
    public void revokedPermissionsChanged() {
        updatePermissionsPreference();
    }

    @Override
    public void notificationPermissionsChanged() {
        updateNotificationsReviewPreference();
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

    private void updateSafeBrowsingPreference() {
        @SafeBrowsingState int safeBrowsingState = mDelegate.getSafeBrowsingState();
        mSafeBrowsingPropertyModel.set(
                SafetyHubModuleProperties.SAFE_BROWSING_STATE, safeBrowsingState);
    }
}
