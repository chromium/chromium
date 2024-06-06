// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.graphics.drawable.Drawable;

import androidx.annotation.NonNull;
import androidx.preference.Preference;

import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

public class SafetyHubModuleViewBinder {
    public static void bindCommonProperties(
            PropertyModel model, Preference preference, PropertyKey propertyKey) {
        if (SafetyHubModuleProperties.IS_VISIBLE == propertyKey) {
            preference.setVisible(model.get(SafetyHubModuleProperties.IS_VISIBLE));
        } else if (SafetyHubModuleProperties.ON_CLICK_LISTENER == propertyKey) {
            Runnable onClickListener = model.get(SafetyHubModuleProperties.ON_CLICK_LISTENER);
            assert onClickListener != null;
            preference.setOnPreferenceClickListener(
                    new Preference.OnPreferenceClickListener() {
                        @Override
                        public boolean onPreferenceClick(@NonNull Preference preference) {
                            onClickListener.run();
                            return true;
                        }
                    });
        }
    }

    public static void bindPasswordCheckProperties(
            PropertyModel model, Preference preference, PropertyKey propertyKey) {
        bindCommonProperties(model, preference, propertyKey);
        if (SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT == propertyKey) {
            updatePasswordCheckModule(preference, model);
        }
    }

    public static void bindUpdateCheckProperties(
            PropertyModel model, Preference preference, PropertyKey propertyKey) {
        bindCommonProperties(model, preference, propertyKey);
        if (SafetyHubModuleProperties.UPDATE_STATUS == propertyKey) {
            updateUpdateCheckModule(preference, model);
        }
    }

    public static void bindPermissionsProperties(
            PropertyModel model, Preference preference, PropertyKey propertyKey) {
        bindCommonProperties(model, preference, propertyKey);
        if (SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT == propertyKey) {
            updatePermissionsModule(preference, model);
        }
    }

    public static void bindNotificationsReviewProperties(
            PropertyModel model, Preference preference, PropertyKey propertyKey) {
        bindCommonProperties(model, preference, propertyKey);
        if (SafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT == propertyKey) {
            updateNotificationsReviewModule(preference, model);
        }
    }

    private static void updatePasswordCheckModule(Preference preference, PropertyModel model) {
        int compromisedPasswordsCount =
                model.get(SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT);
        String title;
        Drawable iconDrawable;
        if (compromisedPasswordsCount > 0) {
            title =
                    preference
                            .getContext()
                            .getResources()
                            .getQuantityString(
                                    R.plurals.safety_check_passwords_compromised_exist,
                                    compromisedPasswordsCount,
                                    compromisedPasswordsCount);

            iconDrawable = getErrorIcon(preference);
        } else {
            title = preference.getContext().getString(R.string.safety_check_passwords_safe);
            iconDrawable = getCheckmarkIcon(preference);
        }
        preference.setTitle(title);
        preference.setIcon(iconDrawable);
    }

    private static void updateUpdateCheckModule(Preference preference, PropertyModel model) {
        UpdateStatusProvider.UpdateStatus updateStatus =
                model.get(SafetyHubModuleProperties.UPDATE_STATUS);

        if (updateStatus == null) {
            preference.setIcon(getCheckmarkIcon(preference));
            preference.setTitle(R.string.safety_check_updates_updated);
            return;
        }

        switch (updateStatus.updateState) {
            case UpdateStatusProvider.UpdateState.UNSUPPORTED_OS_VERSION:
                preference.setIcon(getErrorIcon(preference));
                preference.setTitle(R.string.menu_update_unsupported_summary_default);
                preference.setSummary(updateStatus.latestUnsupportedVersion);
                break;
            case UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE:
                preference.setIcon(getErrorIcon(preference));
                preference.setTitle(R.string.safety_check_updates_outdated);
                break;
            default:
                preference.setIcon(getCheckmarkIcon(preference));
                preference.setTitle(R.string.safety_check_updates_updated);
                preference.setSummary(updateStatus.latestVersion);
        }
    }

    private static void updatePermissionsModule(Preference preference, PropertyModel model) {
        int sitesWithUnusedPermissionsCount =
                model.get(SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT);
        String title;
        Drawable iconDrawable;
        if (sitesWithUnusedPermissionsCount > 0) {
            title =
                    preference
                            .getContext()
                            .getResources()
                            .getQuantityString(
                                    R.plurals.safety_hub_permissions_warning_title,
                                    sitesWithUnusedPermissionsCount,
                                    sitesWithUnusedPermissionsCount);

            iconDrawable = getWarningIcon(preference);
        } else {
            title = preference.getContext().getString(R.string.safety_hub_permissions_ok_title);
            iconDrawable = getCheckmarkIcon(preference);
        }
        preference.setTitle(title);
        preference.setIcon(iconDrawable);
    }

    private static void updateNotificationsReviewModule(
            Preference preference, PropertyModel model) {
        int notificationPermissionsForReviewCount =
                model.get(SafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT);
        String title;
        Drawable iconDrawable;
        if (notificationPermissionsForReviewCount > 0) {
            title =
                    preference
                            .getContext()
                            .getResources()
                            .getQuantityString(
                                    R.plurals.safety_hub_notifications_review_warning_title,
                                    notificationPermissionsForReviewCount,
                                    notificationPermissionsForReviewCount);

            iconDrawable = getWarningIcon(preference);
        } else {
            title =
                    preference
                            .getContext()
                            .getString(R.string.safety_hub_notifications_review_ok_title);
            iconDrawable = getCheckmarkIcon(preference);
        }
        preference.setTitle(title);
        preference.setIcon(iconDrawable);
    }

    private static Drawable getErrorIcon(Preference preference) {
        return SettingsUtils.getTintedIcon(
                preference.getContext(), R.drawable.ic_error, R.color.default_red);
    }

    private static Drawable getWarningIcon(Preference preference) {
        return SettingsUtils.getTintedIcon(
                preference.getContext(),
                R.drawable.btn_info,
                R.color.default_icon_color_secondary_tint_list);
    }

    private static Drawable getCheckmarkIcon(Preference preference) {
        return SettingsUtils.getTintedIcon(
                preference.getContext(), R.drawable.ic_checkmark_24dp, R.color.default_green);
    }
}
