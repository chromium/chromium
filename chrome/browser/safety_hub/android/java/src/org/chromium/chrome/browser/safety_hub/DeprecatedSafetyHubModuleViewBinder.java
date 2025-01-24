// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.safety_hub.DeprecatedSafetyHubModuleProperties.ModuleOption;
import org.chromium.chrome.browser.safety_hub.DeprecatedSafetyHubModuleProperties.ModuleState;
import org.chromium.components.browser_ui.settings.CardPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

public class DeprecatedSafetyHubModuleViewBinder {
    public static void bindCommonProperties(
            PropertyModel model,
            SafetyHubExpandablePreference preference,
            PropertyKey propertyKey) {
        if (DeprecatedSafetyHubModuleProperties.IS_VISIBLE == propertyKey) {
            preference.setVisible(model.get(DeprecatedSafetyHubModuleProperties.IS_VISIBLE));
        } else if (DeprecatedSafetyHubModuleProperties.IS_EXPANDED == propertyKey) {
            preference.setExpanded(model.get(DeprecatedSafetyHubModuleProperties.IS_EXPANDED));
        }
    }

    public static void bindPasswordCheckProperties(
            PropertyModel model,
            SafetyHubExpandablePreference preference,
            PropertyKey propertyKey) {
        bindCommonProperties(model, preference, propertyKey);
        if (DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT == propertyKey
                || DeprecatedSafetyHubModuleProperties.WEAK_PASSWORDS_COUNT == propertyKey
                || DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT == propertyKey
                || DeprecatedSafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY == propertyKey
                || DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN == propertyKey
                || DeprecatedSafetyHubModuleProperties.PRIMARY_BUTTON_LISTENER == propertyKey
                || DeprecatedSafetyHubModuleProperties.SAFE_STATE_BUTTON_LISTENER == propertyKey
                || DeprecatedSafetyHubModuleProperties.ACCOUNT_EMAIL == propertyKey) {
            updatePasswordCheckModule(preference, model);
        }
    }

    public static void bindBrowserStateProperties(
            PropertyModel model, CardPreference preference, PropertyKey propertyKey) {
        if (DeprecatedSafetyHubModuleProperties.SAFE_BROWSING_STATE == propertyKey
                || DeprecatedSafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT
                        == propertyKey
                || DeprecatedSafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT
                        == propertyKey
                || DeprecatedSafetyHubModuleProperties.UPDATE_STATUS == propertyKey
                || DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN == propertyKey
                || DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT == propertyKey
                || DeprecatedSafetyHubModuleProperties.WEAK_PASSWORDS_COUNT == propertyKey
                || DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT == propertyKey) {
            updateBrowserStateModule(preference, model);
        }
    }

    private static void updatePasswordCheckModule(
            SafetyHubExpandablePreference preference, PropertyModel model) {
        SafetyHubPasswordModuleHelper.updatePreference(preference, model);
        @ModuleOption int option = ModuleOption.ACCOUNT_PASSWORDS;
        @ModuleState int state = getModuleState(model, option);
        boolean managed = model.get(DeprecatedSafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY);
        preference.setIcon(getIconForModuleState(preference.getContext(), state, managed));
        preference.setOrder(getOrderForModuleState(option, state, managed));
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
        // DeprecatedSafetyHubModuleProperties.ModuleState}. Modules in warning state that are not
        // controlled
        // by policy should appear first in the list. Followed by unavailable, info then safe
        // states.
        // If multiple modules have the same state, fallback to the order in {@link
        // DeprecatedSafetyHubModuleProperties.ModuleOption}.
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
                        model.get(DeprecatedSafetyHubModuleProperties.UPDATE_STATUS));
            case ModuleOption.UNUSED_PERMISSIONS:
                return SafetyHubUtils.getPermissionsModuleState(
                        model.get(
                                DeprecatedSafetyHubModuleProperties
                                        .SITES_WITH_UNUSED_PERMISSIONS_COUNT));
            case ModuleOption.NOTIFICATION_REVIEW:
                return SafetyHubUtils.getNotificationModuleState(
                        model.get(
                                DeprecatedSafetyHubModuleProperties
                                        .NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT));
            case ModuleOption.SAFE_BROWSING:
                return SafetyHubUtils.getSafeBrowsingModuleState(
                        model.get(DeprecatedSafetyHubModuleProperties.SAFE_BROWSING_STATE));
            default:
                throw new IllegalArgumentException();
        }
    }
}
