// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.preference.Preference;

import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

public class SafetyHubModuleViewBinder {
    public static void bindCommonProperties(
            PropertyModel model,
            SafetyHubExpandablePreference preference,
            PropertyKey propertyKey) {
        if (SafetyHubModuleProperties.IS_VISIBLE == propertyKey) {
            preference.setVisible(model.get(SafetyHubModuleProperties.IS_VISIBLE));
        } else if (SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY == propertyKey) {
            preference.setControlledByPolicy(
                    model.get(SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY));
        }
    }

    public static void bindPasswordCheckProperties(
            PropertyModel model,
            SafetyHubExpandablePreference preference,
            PropertyKey propertyKey) {
        bindCommonProperties(model, preference, propertyKey);
        if (SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT == propertyKey) {
            updatePasswordCheckModule(preference, model);
        }
    }

    public static void bindUpdateCheckProperties(
            PropertyModel model,
            SafetyHubExpandablePreference preference,
            PropertyKey propertyKey) {
        bindCommonProperties(model, preference, propertyKey);
        if (SafetyHubModuleProperties.UPDATE_STATUS == propertyKey) {
            updateUpdateCheckModule(preference, model);
        }
    }

    public static void bindPermissionsProperties(
            PropertyModel model,
            SafetyHubExpandablePreference preference,
            PropertyKey propertyKey) {
        bindCommonProperties(model, preference, propertyKey);
        if (SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT == propertyKey) {
            updatePermissionsModule(preference, model);
        }
    }

    public static void bindNotificationsReviewProperties(
            PropertyModel model,
            SafetyHubExpandablePreference preference,
            PropertyKey propertyKey) {
        bindCommonProperties(model, preference, propertyKey);
        if (SafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT == propertyKey) {
            updateNotificationsReviewModule(preference, model);
        }
    }

    public static void bindSafeBrowsingProperties(
            PropertyModel model,
            SafetyHubExpandablePreference preference,
            PropertyKey propertyKey) {
        bindCommonProperties(model, preference, propertyKey);
        if (SafetyHubModuleProperties.SAFE_BROWSING_STATE == propertyKey
                || SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY == propertyKey) {
            updateSafeBrowsingModule(preference, model);
        }
    }

    private static void updateSafeBrowsingModule(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        @SafeBrowsingState
        int safeBrowsingState = model.get(SafetyHubModuleProperties.SAFE_BROWSING_STATE);
        boolean managed = model.get(SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY);

        String title;
        String summary;
        Drawable iconDrawable;
        String primaryButtonText = null;
        String secondaryButtonText = null;
        View.OnClickListener primaryButtonListener = null;
        View.OnClickListener secondaryButtonListener = null;
        boolean expanded = false;

        switch (safeBrowsingState) {
            case SafeBrowsingState.STANDARD_PROTECTION:
                title =
                        preference
                                .getContext()
                                .getString(R.string.safety_hub_safe_browsing_on_title);
                iconDrawable = getCheckmarkIcon(preference);

                if (managed) {
                    summary =
                            preference
                                    .getContext()
                                    .getString(
                                            R.string.safety_hub_safe_browsing_on_summary_managed);
                } else {
                    summary =
                            preference
                                    .getContext()
                                    .getString(R.string.safety_hub_safe_browsing_on_summary);
                    secondaryButtonText =
                            preference
                                    .getContext()
                                    .getString(R.string.safety_hub_go_to_security_settings_button);
                    secondaryButtonListener =
                            model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER);
                }
                break;
            case SafeBrowsingState.ENHANCED_PROTECTION:
                title =
                        preference
                                .getContext()
                                .getString(R.string.safety_hub_safe_browsing_enhanced_title);
                iconDrawable = getCheckmarkIcon(preference);

                if (managed) {
                    summary =
                            preference
                                    .getContext()
                                    .getString(
                                            R.string
                                                    .safety_hub_safe_browsing_enhanced_summary_managed);
                } else {
                    summary =
                            preference
                                    .getContext()
                                    .getString(R.string.safety_hub_safe_browsing_enhanced_summary);
                    secondaryButtonText =
                            preference
                                    .getContext()
                                    .getString(R.string.safety_hub_go_to_security_settings_button);
                    secondaryButtonListener =
                            model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER);
                }
                break;
            default:
                title =
                        preference
                                .getContext()
                                .getString(R.string.prefs_safe_browsing_no_protection_summary);

                if (managed) {
                    iconDrawable = getCheckmarkIcon(preference);
                    summary =
                            preference
                                    .getContext()
                                    .getString(
                                            R.string.safety_hub_safe_browsing_off_summary_managed);
                } else {
                    iconDrawable = getErrorIcon(preference);
                    summary =
                            preference
                                    .getContext()
                                    .getString(R.string.safety_hub_safe_browsing_off_summary);
                    primaryButtonText =
                            preference.getContext().getString(R.string.safety_hub_turn_on_button);
                    primaryButtonListener =
                            model.get(SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER);
                    expanded = true;
                }
        }

        preference.setIcon(iconDrawable);
        preference.setTitle(title);
        preference.setSummary(summary);
        preference.setPrimaryButtonText(primaryButtonText);
        preference.setSecondaryButtonText(secondaryButtonText);
        preference.setPrimaryButtonClickListener(primaryButtonListener);
        preference.setSecondaryButtonClickListener(secondaryButtonListener);
        preference.setExpanded(expanded);
    }

    private static void updatePasswordCheckModule(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        int compromisedPasswordsCount =
                model.get(SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT);
        String title;
        Drawable iconDrawable;
        String primaryButtonText = null;
        String secondaryButtonText = null;
        View.OnClickListener primaryButtonListener = null;
        View.OnClickListener secondaryButtonListener = null;
        boolean expanded = false;

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
            primaryButtonText =
                    preference
                            .getContext()
                            .getString(R.string.safety_hub_passwords_navigation_button);
            primaryButtonListener = model.get(SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER);
            expanded = true;
        } else {
            title = preference.getContext().getString(R.string.safety_check_passwords_safe);
            iconDrawable = getCheckmarkIcon(preference);
            secondaryButtonText =
                    preference
                            .getContext()
                            .getString(R.string.safety_hub_passwords_navigation_button);
            secondaryButtonListener =
                    model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER);
        }
        preference.setTitle(title);
        preference.setIcon(iconDrawable);
        preference.setPrimaryButtonText(primaryButtonText);
        preference.setSecondaryButtonText(secondaryButtonText);
        preference.setPrimaryButtonClickListener(primaryButtonListener);
        preference.setSecondaryButtonClickListener(secondaryButtonListener);
        preference.setExpanded(expanded);
    }

    private static void updateUpdateCheckModule(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        UpdateStatusProvider.UpdateStatus updateStatus =
                model.get(SafetyHubModuleProperties.UPDATE_STATUS);

        String title;
        String summary = null;
        Drawable iconDrawable;
        String primaryButtonText = null;
        String secondaryButtonText = null;
        View.OnClickListener primaryButtonListener = null;
        View.OnClickListener secondaryButtonListener = null;
        boolean expanded = false;

        if (updateStatus == null) {
            title = preference.getContext().getString(R.string.safety_check_updates_updated);
            iconDrawable = getCheckmarkIcon(preference);
            secondaryButtonText =
                    preference.getContext().getString(R.string.safety_hub_go_to_google_play_button);
            secondaryButtonListener =
                    model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER);
        } else {
            switch (updateStatus.updateState) {
                case UpdateStatusProvider.UpdateState.UNSUPPORTED_OS_VERSION:
                    title =
                            preference
                                    .getContext()
                                    .getString(R.string.menu_update_unsupported_summary_default);
                    summary = updateStatus.latestUnsupportedVersion;
                    iconDrawable = getErrorIcon(preference);
                    expanded = true;
                    break;
                case UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE:
                    title =
                            preference
                                    .getContext()
                                    .getString(R.string.safety_check_updates_outdated);
                    iconDrawable = getErrorIcon(preference);
                    primaryButtonText = preference.getContext().getString(R.string.menu_update);
                    primaryButtonListener =
                            model.get(SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER);
                    expanded = true;
                    break;
                default:
                    title =
                            preference
                                    .getContext()
                                    .getString(R.string.safety_check_updates_updated);
                    summary = updateStatus.latestVersion;
                    iconDrawable = getCheckmarkIcon(preference);
                    secondaryButtonText =
                            preference
                                    .getContext()
                                    .getString(R.string.safety_hub_go_to_google_play_button);
                    secondaryButtonListener =
                            model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER);
            }
        }

        preference.setIcon(iconDrawable);
        preference.setTitle(title);
        preference.setSummary(summary);
        preference.setPrimaryButtonText(primaryButtonText);
        preference.setSecondaryButtonText(secondaryButtonText);
        preference.setPrimaryButtonClickListener(primaryButtonListener);
        preference.setSecondaryButtonClickListener(secondaryButtonListener);
        preference.setExpanded(expanded);
    }

    private static void updatePermissionsModule(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        int sitesWithUnusedPermissionsCount =
                model.get(SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT);
        String title;
        Drawable iconDrawable;
        String primaryButtonText = null;
        String secondaryButtonText;
        View.OnClickListener primaryButtonListener = null;
        View.OnClickListener secondaryButtonListener = null;
        boolean expanded = false;

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
            primaryButtonText = preference.getContext().getString(R.string.got_it);
            secondaryButtonText =
                    preference.getContext().getString(R.string.safety_hub_view_sites_button);
            primaryButtonListener = model.get(SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER);
            secondaryButtonListener =
                    model.get(SafetyHubModuleProperties.SECONDARY_BUTTON_LISTENER);
            expanded = true;
        } else {
            title = preference.getContext().getString(R.string.safety_hub_permissions_ok_title);
            iconDrawable = getCheckmarkIcon(preference);
            secondaryButtonText =
                    preference.getContext().getString(R.string.safety_hub_go_to_settings_button);
            secondaryButtonListener =
                    model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER);
        }
        preference.setTitle(title);
        preference.setIcon(iconDrawable);
        preference.setPrimaryButtonText(primaryButtonText);
        preference.setSecondaryButtonText(secondaryButtonText);
        preference.setPrimaryButtonClickListener(primaryButtonListener);
        preference.setSecondaryButtonClickListener(secondaryButtonListener);
        preference.setExpanded(expanded);
    }

    private static void updateNotificationsReviewModule(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        int notificationPermissionsForReviewCount =
                model.get(SafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT);
        String title;
        Drawable iconDrawable;
        String primaryButtonText = null;
        String secondaryButtonText;
        View.OnClickListener primaryButtonListener = null;
        View.OnClickListener secondaryButtonListener = null;
        boolean expanded = false;

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
            primaryButtonText =
                    preference
                            .getContext()
                            .getString(R.string.safety_hub_notifications_reset_all_button);
            secondaryButtonText =
                    preference.getContext().getString(R.string.safety_hub_view_sites_button);
            primaryButtonListener = model.get(SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER);
            secondaryButtonListener =
                    model.get(SafetyHubModuleProperties.SECONDARY_BUTTON_LISTENER);
            expanded = true;
        } else {
            title =
                    preference
                            .getContext()
                            .getString(R.string.safety_hub_notifications_review_ok_title);
            iconDrawable = getCheckmarkIcon(preference);
            secondaryButtonText =
                    preference
                            .getContext()
                            .getString(R.string.safety_hub_go_to_notifications_button);
            secondaryButtonListener =
                    model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER);
        }
        preference.setTitle(title);
        preference.setIcon(iconDrawable);
        preference.setPrimaryButtonText(primaryButtonText);
        preference.setSecondaryButtonText(secondaryButtonText);
        preference.setPrimaryButtonClickListener(primaryButtonListener);
        preference.setSecondaryButtonClickListener(secondaryButtonListener);
        preference.setExpanded(expanded);
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
