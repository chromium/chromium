// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.BuildInfo;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleProperties.ModuleOption;
import org.chromium.chrome.browser.safety_hub.SafetyHubModuleProperties.ModuleState;
import org.chromium.components.browser_ui.settings.CardPreference;
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
        } else if (SafetyHubModuleProperties.IS_EXPANDED == propertyKey) {
            preference.setExpanded(model.get(SafetyHubModuleProperties.IS_EXPANDED));
        }
    }

    public static void bindPasswordCheckProperties(
            PropertyModel model,
            SafetyHubExpandablePreference preference,
            PropertyKey propertyKey) {
        bindCommonProperties(model, preference, propertyKey);
        if (SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT == propertyKey
                || SafetyHubModuleProperties.WEAK_PASSWORDS_COUNT == propertyKey
                || SafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT == propertyKey
                || SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY == propertyKey
                || SafetyHubModuleProperties.IS_SIGNED_IN == propertyKey
                || SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER == propertyKey
                || SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER == propertyKey
                || SafetyHubModuleProperties.ACCOUNT_EMAIL == propertyKey) {
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

    public static void bindBrowserStateProperties(
            PropertyModel model, CardPreference preference, PropertyKey propertyKey) {
        if (SafetyHubModuleProperties.SAFE_BROWSING_STATE == propertyKey
                || SafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT
                        == propertyKey
                || SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT == propertyKey
                || SafetyHubModuleProperties.UPDATE_STATUS == propertyKey
                || SafetyHubModuleProperties.IS_SIGNED_IN == propertyKey
                || SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT == propertyKey
                || SafetyHubModuleProperties.WEAK_PASSWORDS_COUNT == propertyKey
                || SafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT == propertyKey) {
            updateBrowserStateModule(preference, model);
        }
    }

    private static void updateSafeBrowsingModule(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        @ModuleOption int option = ModuleOption.SAFE_BROWSING;
        @SafeBrowsingState
        int safeBrowsingState = model.get(SafetyHubModuleProperties.SAFE_BROWSING_STATE);
        boolean managed = model.get(SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY);
        @ModuleState int state = getModuleState(model, option);
        String title;
        String summary;
        String primaryButtonText = null;
        String secondaryButtonText =
                preference
                        .getContext()
                        .getString(R.string.safety_hub_go_to_security_settings_button);
        View.OnClickListener primaryButtonListener = null;
        View.OnClickListener secondaryButtonListener =
                model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER);

        switch (safeBrowsingState) {
            case SafeBrowsingState.STANDARD_PROTECTION:
                title =
                        preference
                                .getContext()
                                .getString(R.string.safety_hub_safe_browsing_on_title);
                summary =
                        preference
                                .getContext()
                                .getString(
                                        managed
                                                ? R.string
                                                        .safety_hub_safe_browsing_on_summary_managed
                                                : R.string.safety_hub_safe_browsing_on_summary);
                break;
            case SafeBrowsingState.ENHANCED_PROTECTION:
                title =
                        preference
                                .getContext()
                                .getString(R.string.safety_hub_safe_browsing_enhanced_title);
                summary =
                        preference
                                .getContext()
                                .getString(
                                        managed
                                                ? R.string
                                                        .safety_hub_safe_browsing_enhanced_summary_managed
                                                : R.string
                                                        .safety_hub_safe_browsing_enhanced_summary);
                break;
            default:
                title =
                        preference
                                .getContext()
                                .getString(R.string.prefs_safe_browsing_no_protection_summary);

                summary =
                        preference
                                .getContext()
                                .getString(
                                        managed
                                                ? R.string
                                                        .safety_hub_safe_browsing_off_summary_managed
                                                : R.string.safety_hub_safe_browsing_off_summary);

                if (!managed) {
                    secondaryButtonText = null;
                    secondaryButtonListener = null;
                    primaryButtonText =
                            preference.getContext().getString(R.string.safety_hub_turn_on_button);
                    primaryButtonListener =
                            model.get(SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER);
                }
        }

        preference.setTitle(title);
        preference.setSummary(summary);
        preference.setPrimaryButtonText(primaryButtonText);
        preference.setSecondaryButtonText(secondaryButtonText);
        preference.setPrimaryButtonClickListener(primaryButtonListener);
        preference.setSecondaryButtonClickListener(secondaryButtonListener);

        preference.setIcon(getIconForModuleState(preference.getContext(), state, managed));
        preference.setOrder(getOrderForModuleState(option, state, managed));
    }

    private static void updatePasswordCheckModule(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        SafetyHubPasswordModuleHelper.updatePreference(preference, model);
        @ModuleOption int option = ModuleOption.ACCOUNT_PASSWORDS;
        @ModuleState int state = getModuleState(model, option);
        boolean managed = model.get(SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY);
        preference.setIcon(getIconForModuleState(preference.getContext(), state, managed));
        preference.setOrder(getOrderForModuleState(option, state, managed));
    }

    private static void updateUpdateCheckModule(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        @ModuleOption int option = ModuleOption.UPDATE_CHECK;
        UpdateStatusProvider.UpdateStatus updateStatus =
                model.get(SafetyHubModuleProperties.UPDATE_STATUS);
        @ModuleState int state = getModuleState(model, option);
        String title;
        String summary = null;
        String primaryButtonText = null;
        String secondaryButtonText = null;
        View.OnClickListener primaryButtonListener = null;
        View.OnClickListener secondaryButtonListener = null;

        if (updateStatus == null) {
            title = preference.getContext().getString(R.string.safety_hub_update_unavailable_title);
            summary = preference.getContext().getString(R.string.safety_hub_unavailable_summary);
            secondaryButtonText =
                    preference.getContext().getString(R.string.safety_hub_go_to_google_play_button);
            secondaryButtonListener =
                    model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER);
        } else {
            switch (updateStatus.updateState) {
                case UpdateStatusProvider.UpdateState.UNSUPPORTED_OS_VERSION:
                    title = preference.getContext().getString(R.string.menu_update_unsupported);
                    summary =
                            preference
                                    .getContext()
                                    .getString(R.string.menu_update_unsupported_summary_default);
                    break;
                case UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE:
                    title =
                            preference
                                    .getContext()
                                    .getString(R.string.safety_check_updates_outdated);
                    summary =
                            preference
                                    .getContext()
                                    .getString(R.string.safety_hub_updates_outdated_summary);
                    primaryButtonText = preference.getContext().getString(R.string.menu_update);
                    primaryButtonListener =
                            model.get(SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER);
                    break;
                default:
                    title =
                            preference
                                    .getContext()
                                    .getString(R.string.safety_check_updates_updated);
                    String currentVersion = BuildInfo.getInstance().versionName;
                    if (currentVersion != null && !currentVersion.isEmpty()) {
                        summary =
                                preference
                                        .getContext()
                                        .getString(
                                                R.string.safety_hub_version_summary,
                                                currentVersion);
                    }
                    secondaryButtonText =
                            preference
                                    .getContext()
                                    .getString(R.string.safety_hub_go_to_google_play_button);
                    secondaryButtonListener =
                            model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER);
            }
        }

        preference.setTitle(title);
        preference.setSummary(summary);
        preference.setPrimaryButtonText(primaryButtonText);
        preference.setSecondaryButtonText(secondaryButtonText);
        preference.setPrimaryButtonClickListener(primaryButtonListener);
        preference.setSecondaryButtonClickListener(secondaryButtonListener);

        preference.setIcon(getIconForModuleState(preference.getContext(), state, false));
        preference.setOrder(getOrderForModuleState(option, state, false));
    }

    private static void updatePermissionsModule(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        @ModuleOption int option = ModuleOption.UNUSED_PERMISSIONS;
        int sitesWithUnusedPermissionsCount =
                model.get(SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT);
        @ModuleState int state = getModuleState(model, option);
        String title;
        String summary;
        String primaryButtonText = null;
        String secondaryButtonText;
        View.OnClickListener primaryButtonListener = null;
        View.OnClickListener secondaryButtonListener = null;

        if (sitesWithUnusedPermissionsCount > 0) {
            title =
                    preference
                            .getContext()
                            .getResources()
                            .getQuantityString(
                                    R.plurals.safety_hub_permissions_warning_title,
                                    sitesWithUnusedPermissionsCount,
                                    sitesWithUnusedPermissionsCount);
            summary =
                    preference
                            .getContext()
                            .getString(R.string.safety_hub_permissions_warning_summary);
            primaryButtonText = preference.getContext().getString(R.string.got_it);
            secondaryButtonText =
                    preference.getContext().getString(R.string.safety_hub_view_sites_button);
            primaryButtonListener = model.get(SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER);
            secondaryButtonListener =
                    model.get(SafetyHubModuleProperties.SECONDARY_BUTTON_LISTENER);
        } else {
            title = preference.getContext().getString(R.string.safety_hub_permissions_ok_title);
            summary = preference.getContext().getString(R.string.safety_hub_permissions_ok_summary);
            secondaryButtonText =
                    preference
                            .getContext()
                            .getString(R.string.safety_hub_go_to_site_settings_button);
            secondaryButtonListener =
                    model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER);
        }

        preference.setTitle(title);
        preference.setSummary(summary);
        preference.setPrimaryButtonText(primaryButtonText);
        preference.setSecondaryButtonText(secondaryButtonText);
        preference.setPrimaryButtonClickListener(primaryButtonListener);
        preference.setSecondaryButtonClickListener(secondaryButtonListener);

        preference.setIcon(getIconForModuleState(preference.getContext(), state, false));
        preference.setOrder(getOrderForModuleState(option, state, false));
    }

    private static void updateNotificationsReviewModule(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        @ModuleOption int option = ModuleOption.NOTIFICATION_REVIEW;
        int notificationPermissionsForReviewCount =
                model.get(SafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT);
        @ModuleState int state = getModuleState(model, option);
        String title;
        String summary;
        String primaryButtonText = null;
        String secondaryButtonText;
        View.OnClickListener primaryButtonListener = null;
        View.OnClickListener secondaryButtonListener = null;

        if (notificationPermissionsForReviewCount > 0) {
            title =
                    preference
                            .getContext()
                            .getResources()
                            .getQuantityString(
                                    R.plurals.safety_hub_notifications_review_warning_title,
                                    notificationPermissionsForReviewCount,
                                    notificationPermissionsForReviewCount);
            summary =
                    preference
                            .getContext()
                            .getString(R.string.safety_hub_notifications_review_warning_summary);
            primaryButtonText =
                    preference
                            .getContext()
                            .getString(R.string.safety_hub_notifications_reset_all_button);
            secondaryButtonText =
                    preference.getContext().getString(R.string.safety_hub_view_sites_button);
            primaryButtonListener = model.get(SafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER);
            secondaryButtonListener =
                    model.get(SafetyHubModuleProperties.SECONDARY_BUTTON_LISTENER);
        } else {
            title =
                    preference
                            .getContext()
                            .getString(R.string.safety_hub_notifications_review_ok_title);
            summary =
                    preference
                            .getContext()
                            .getString(R.string.safety_hub_notifications_review_ok_summary);
            secondaryButtonText =
                    preference
                            .getContext()
                            .getString(R.string.safety_hub_go_to_notification_settings_button);
            secondaryButtonListener =
                    model.get(SafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER);
        }

        preference.setTitle(title);
        preference.setSummary(summary);
        preference.setPrimaryButtonText(primaryButtonText);
        preference.setSecondaryButtonText(secondaryButtonText);
        preference.setPrimaryButtonClickListener(primaryButtonListener);
        preference.setSecondaryButtonClickListener(secondaryButtonListener);

        preference.setIcon(getIconForModuleState(preference.getContext(), state, false));
        preference.setOrder(getOrderForModuleState(option, state, false));
    }

    private static void updateBrowserStateModule(CardPreference preference, PropertyModel model) {
        if (!isBrowserStateSafe(model)) {
            preference.setVisible(false);
            return;
        }

        preference.setTitle(R.string.safety_hub_safe_browser_state_title);
        preference.setSummary(
                preference.getContext().getString(R.string.safety_hub_checked_recently));
        preference.setIconDrawable(
                AppCompatResources.getDrawable(
                        preference.getContext(), R.drawable.ic_check_circle_filled_green_24dp));
        preference.setShouldCenterIcon(true);
        preference.setCloseIconVisibility(View.GONE);
        preference.setVisible(true);
    }

    private static Drawable getIconForModuleState(
            Context context, @ModuleState int state, boolean managed) {
        switch (state) {
            case ModuleState.SAFE:
                return SettingsUtils.getTintedIcon(
                        context, R.drawable.material_ic_check_24dp, R.color.default_green);
            case ModuleState.INFO:
            case ModuleState.UNAVAILABLE:
                return managed
                        ? getManagedIcon(context)
                        : SettingsUtils.getTintedIcon(
                                context,
                                R.drawable.btn_info,
                                R.color.default_icon_color_secondary_tint_list);
            case ModuleState.WARNING:
                return managed
                        ? getManagedIcon(context)
                        : SettingsUtils.getTintedIcon(
                                context, R.drawable.ic_error, R.color.default_red);
            default:
                throw new IllegalArgumentException();
        }
    }

    private static Drawable getManagedIcon(Context context) {
        return SettingsUtils.getTintedIcon(
                context, R.drawable.ic_business, R.color.default_icon_color_secondary_tint_list);
    }

    private static int getOrderForModuleState(
            @ModuleOption int option, @ModuleState int state, boolean managed) {
        // Modules are ordered based on the severity of their {@link
        // SafetyHubModuleProperties.ModuleState}. Modules in warning state that are not controlled
        // by policy should appear first in the list. Followed by unavailable, info then safe
        // states.
        // If multiple modules have the same state, fallback to the order in {@link
        // SafetyHubModuleProperties.ModuleOption}.
        switch (state) {
            case ModuleState.SAFE:
            case ModuleState.INFO:
            case ModuleState.UNAVAILABLE:
                return option + (state * ModuleOption.NUM_ENTRIES);
            case ModuleState.WARNING:
                return option
                        + (managed
                                ? (ModuleState.INFO * ModuleOption.NUM_ENTRIES)
                                : (ModuleState.WARNING * ModuleOption.NUM_ENTRIES));
            default:
                throw new IllegalArgumentException();
        }
    }

    static boolean isBrowserStateSafe(PropertyModel model) {
        for (@ModuleOption int i = ModuleOption.OPTION_FIRST; i < ModuleOption.NUM_ENTRIES; i++) {
            if (getModuleState(model, i) < ModuleState.INFO) {
                return false;
            }
        }
        return true;
    }

    static @ModuleState int getModuleState(PropertyModel model, @ModuleOption int option) {
        switch (option) {
            case ModuleOption.ACCOUNT_PASSWORDS:
                return SafetyHubPasswordModuleHelper.getModuleState(model);
            case ModuleOption.UPDATE_CHECK:
                return SafetyHubUtils.getUpdateCheckModuleState(
                        model.get(SafetyHubModuleProperties.UPDATE_STATUS));
            case ModuleOption.UNUSED_PERMISSIONS:
                return SafetyHubUtils.getPermissionsModuleState(
                        model.get(SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT));
            case ModuleOption.NOTIFICATION_REVIEW:
                return SafetyHubUtils.getNotificationModuleState(
                        model.get(
                                SafetyHubModuleProperties
                                        .NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT));
            case ModuleOption.SAFE_BROWSING:
                return SafetyHubUtils.getSafeBrowsingModuleState(
                        model.get(SafetyHubModuleProperties.SAFE_BROWSING_STATE));
            default:
                throw new IllegalArgumentException();
        }
    }
}
