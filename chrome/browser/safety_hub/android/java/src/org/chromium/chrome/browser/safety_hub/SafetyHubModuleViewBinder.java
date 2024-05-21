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

            iconDrawable =
                    SettingsUtils.getTintedIcon(
                            preference.getContext(), R.drawable.ic_error, R.color.default_red_dark);
        } else {
            title = preference.getContext().getString(R.string.safety_check_passwords_safe);

            iconDrawable =
                    SettingsUtils.getTintedIcon(
                            preference.getContext(), model.get(SafetyHubModuleProperties.ICON));
        }
        preference.setTitle(title);
        preference.setIcon(iconDrawable);
    }

    private static void updateUpdateCheckModule(Preference preference, PropertyModel model) {
        UpdateStatusProvider.UpdateStatus updateStatus =
                model.get(SafetyHubModuleProperties.UPDATE_STATUS);

        if (updateStatus == null) {
            preference.setIcon(
                    SettingsUtils.getTintedIcon(
                            preference.getContext(), model.get(SafetyHubModuleProperties.ICON)));
            preference.setTitle(R.string.safety_check_updates_updated);
            return;
        }

        switch (updateStatus.updateState) {
            case UpdateStatusProvider.UpdateState.UNSUPPORTED_OS_VERSION:
                preference.setIcon(
                        SettingsUtils.getTintedIcon(
                                preference.getContext(),
                                R.drawable.ic_error,
                                R.color.default_red_dark));
                preference.setTitle(R.string.menu_update_unsupported_summary_default);
                preference.setSummary(updateStatus.latestUnsupportedVersion);
                break;
            case UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE:
                preference.setIcon(
                        SettingsUtils.getTintedIcon(
                                preference.getContext(),
                                R.drawable.ic_error,
                                R.color.default_red_dark));
                preference.setTitle(R.string.safety_check_updates_outdated);
                break;
            default:
                preference.setIcon(
                        SettingsUtils.getTintedIcon(
                                preference.getContext(),
                                model.get(SafetyHubModuleProperties.ICON)));
                preference.setTitle(R.string.safety_check_updates_updated);
                preference.setSummary(updateStatus.latestVersion);
        }
    }
}
