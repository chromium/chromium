// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import android.content.Context;
import android.text.TextUtils;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.chrome.browser.safety_check.PasswordsCheckPreferenceProperties.PasswordsState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.SafeBrowsingState;
import org.chromium.chrome.browser.safety_check.SafetyCheckProperties.UpdatesState;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

class SafetyCheckViewBinder {
    public static final String PASSWORDS_KEY_ACCOUNT = "passwords_account";
    public static final String PASSWORDS_KEY_LOCAL = "passwords_local";
    private static final String SAFE_BROWSING_KEY = "safe_browsing";
    private static final String UPDATES_KEY = "updates";
    private static final long MIN_TO_MS = 60 * 1000;
    private static final long H_TO_MS = 60 * MIN_TO_MS;
    private static final long DAY_TO_MS = 24 * H_TO_MS;

    private static String getStringForPasswords(
            Context context, PropertyModel model, @PasswordsState int state) {
        switch (state) {
            case PasswordsState.UNCHECKED:
            case PasswordsState.CHECKING:
                return "";
            case PasswordsState.NO_PASSWORDS:
                return context.getString(R.string.safety_check_passwords_no_passwords);
            case PasswordsState.SIGNED_OUT:
                return context.getString(R.string.safety_check_passwords_error_signed_out);
            case PasswordsState.QUOTA_LIMIT:
                return context.getString(R.string.safety_check_passwords_error_quota_limit);
            case PasswordsState.OFFLINE:
                return context.getString(R.string.safety_check_passwords_error_offline);
            case PasswordsState.ERROR:
                return context.getString(R.string.safety_check_passwords_error);
            case PasswordsState.SAFE:
                return context.getString(R.string.safety_check_passwords_safe);
            case PasswordsState.COMPROMISED_EXIST:
                int compromised =
                        model.get(PasswordsCheckPreferenceProperties.COMPROMISED_PASSWORDS_COUNT);
                return context.getResources()
                        .getQuantityString(
                                R.plurals.safety_check_passwords_compromised_exist,
                                compromised,
                                compromised);
            case PasswordsState.BACKEND_VERSION_NOT_SUPPORTED:
                return context.getString(R.string.safety_check_passwords_update_play_services);
            default:
                assert false : "Unknown PasswordsState value.";
        }
        // Not reached.
        return "";
    }

    private static int getStatusIconForPasswords(@PasswordsState int state) {
        switch (state) {
            case PasswordsState.UNCHECKED:
            case PasswordsState.CHECKING:
                return 0;
            case PasswordsState.SAFE:
                return R.drawable.ic_done_blue;
            case PasswordsState.COMPROMISED_EXIST:
                return R.drawable.ic_warning_red_24dp;
            case PasswordsState.NO_PASSWORDS:
            case PasswordsState.SIGNED_OUT:
            case PasswordsState.QUOTA_LIMIT:
            case PasswordsState.OFFLINE:
            case PasswordsState.ERROR:
            case PasswordsState.BACKEND_VERSION_NOT_SUPPORTED:
                return R.drawable.ic_info_outline_grey_24dp;
            default:
                assert false : "Unknown PasswordsState value.";
        }
        // Not reached.
        return 0;
    }

    private static int getStringForSafeBrowsing(@SafeBrowsingState int state) {
        switch (state) {
            case SafeBrowsingState.UNCHECKED:
            case SafeBrowsingState.CHECKING:
                return 0;
            case SafeBrowsingState.ENABLED_STANDARD:
                return R.string.safety_check_safe_browsing_enabled_standard;
            case SafeBrowsingState.ENABLED_ENHANCED:
                return R.string.safety_check_safe_browsing_enabled_enhanced;
            case SafeBrowsingState.DISABLED:
                return R.string.safety_check_safe_browsing_disabled;
            case SafeBrowsingState.DISABLED_BY_ADMIN:
                return R.string.safety_check_safe_browsing_disabled_by_admin;
            case SafeBrowsingState.ERROR:
                return R.string.safety_check_error;
            default:
                assert false : "Unknown SafeBrowsingState value.";
        }
        // Not reached.
        return 0;
    }

    private static int getStatusIconForSafeBrowsing(@SafeBrowsingState int state) {
        switch (state) {
            case SafeBrowsingState.UNCHECKED:
            case SafeBrowsingState.CHECKING:
                return 0;
            case SafeBrowsingState.ENABLED_STANDARD:
            case SafeBrowsingState.ENABLED_ENHANCED:
                return R.drawable.ic_done_blue;
            case SafeBrowsingState.DISABLED:
            case SafeBrowsingState.ERROR:
                return R.drawable.ic_info_outline_grey_24dp;
            case SafeBrowsingState.DISABLED_BY_ADMIN:
                return R.drawable.ic_business;
            default:
                assert false : "Unknown SafeBrowsingState value.";
        }
        // Not reached.
        return 0;
    }

    private static int getStringForUpdates(@UpdatesState int state) {
        switch (state) {
            case UpdatesState.UNCHECKED:
            case UpdatesState.CHECKING:
                return 0;
            case UpdatesState.UPDATED:
                return R.string.safety_check_updates_updated;
            case UpdatesState.OUTDATED:
                return R.string.safety_check_updates_outdated;
            case UpdatesState.OFFLINE:
                return R.string.safety_check_updates_offline;
            case UpdatesState.ERROR:
                return R.string.safety_check_updates_error;
            default:
                assert false : "Unknown UpdatesState value.";
        }
        // Not reached.
        return 0;
    }

    private static int getStatusIconForUpdates(@UpdatesState int state) {
        switch (state) {
            case UpdatesState.UNCHECKED:
            case UpdatesState.CHECKING:
                return 0;
            case UpdatesState.UPDATED:
                return R.drawable.ic_done_blue;
            case UpdatesState.OUTDATED:
                return R.drawable.ic_warning_red_24dp;
            case UpdatesState.OFFLINE:
            case UpdatesState.ERROR:
                return R.drawable.ic_info_outline_grey_24dp;
            default:
                assert false : "Unknown UpdatesState value.";
        }
        // Not reached.
        return 0;
    }

    /**
     * Generates a String representing how long ago the Safety check was performed last time.
     * @param context A {@link Context} instance to extract the strings.
     * @param lastRunTime A long representing the last run timestamp in milliseconds.
     * @param currentTime A long representing current time in milliseconds.
     * @return A string to display in the UI for the last run timestamp.
     */
    @VisibleForTesting
    static String getLastRunTimestampText(Context context, long lastRunTime, long currentTime) {
        if (lastRunTime == 0) {
            return "";
        }
        long timeDiff = currentTime - lastRunTime;
        if (timeDiff < MIN_TO_MS) {
            return context.getString(R.string.safety_check_timestamp_after);
        } else if (timeDiff < H_TO_MS) {
            int minutes = (int) (timeDiff / MIN_TO_MS);
            return context.getResources()
                    .getQuantityString(
                            R.plurals.safety_check_timestamp_after_mins, minutes, minutes);
        } else if (timeDiff < DAY_TO_MS) {
            int hours = (int) (timeDiff / H_TO_MS);
            return context.getResources()
                    .getQuantityString(R.plurals.safety_check_timestamp_after_hours, hours, hours);
        } else if (timeDiff < 2 * DAY_TO_MS) {
            return context.getString(R.string.safety_check_timestamp_after_yesterday);
        } else {
            int days = (int) (timeDiff / DAY_TO_MS);
            return context.getResources()
                    .getQuantityString(R.plurals.safety_check_timestamp_after_days, days, days);
        }
    }

    private static void displayTimestampText(
            PropertyModel model, SafetyCheckSettingsFragment fragment) {
        long lastRunTime = model.get(SafetyCheckProperties.LAST_RUN_TIMESTAMP);
        long currentTime = System.currentTimeMillis();
        String timestampText =
                getLastRunTimestampText(fragment.getContext(), lastRunTime, currentTime);
        if (!TextUtils.equals(fragment.getTimestampTextView().getText(), timestampText)) {
            fragment.getTimestampTextView().setText(timestampText);
            fragment.getTimestampTextView().announceForAccessibility(timestampText);
        }
    }

    private static void clearTimestampText(SafetyCheckSettingsFragment fragment) {
        fragment.getTimestampTextView().setText("");
    }

    static void bind(
            PropertyModel model, SafetyCheckSettingsFragment fragment, PropertyKey propertyKey) {
        if (SafetyCheckProperties.SAFE_BROWSING_STATE == propertyKey) {
            @SafeBrowsingState int state = model.get(SafetyCheckProperties.SAFE_BROWSING_STATE);
            fragment.updateElementStatus(SAFE_BROWSING_KEY, getStringForSafeBrowsing(state));
            SafetyCheckElementPreference preference = fragment.findPreference(SAFE_BROWSING_KEY);
            preference.setEnabled(true);
            if (state == SafeBrowsingState.UNCHECKED) {
                preference.clearStatusIndicator();
                preference.setEnabled(true);
            } else if (state == SafeBrowsingState.CHECKING) {
                clearTimestampText(fragment);
                preference.showProgressBar();
                preference.setEnabled(false);
            } else {
                displayTimestampText(model, fragment);
                preference.showStatusIcon(getStatusIconForSafeBrowsing(state));
                preference.setEnabled(true);
            }
        } else if (SafetyCheckProperties.UPDATES_STATE == propertyKey) {
            @UpdatesState int state = model.get(SafetyCheckProperties.UPDATES_STATE);
            fragment.updateElementStatus(UPDATES_KEY, getStringForUpdates(state));
            SafetyCheckElementPreference preference = fragment.findPreference(UPDATES_KEY);
            preference.setEnabled(true);
            if (state == UpdatesState.UNCHECKED) {
                preference.clearStatusIndicator();
                preference.setEnabled(true);
            } else if (state == UpdatesState.CHECKING) {
                clearTimestampText(fragment);
                preference.showProgressBar();
                preference.setEnabled(false);
            } else {
                displayTimestampText(model, fragment);
                preference.showStatusIcon(getStatusIconForUpdates(state));
                preference.setEnabled(true);
            }
        } else if (SafetyCheckProperties.SAFE_BROWSING_CLICK_LISTENER == propertyKey) {
            fragment.findPreference(SAFE_BROWSING_KEY)
                    .setOnPreferenceClickListener(
                            (Preference.OnPreferenceClickListener)
                                    model.get(SafetyCheckProperties.SAFE_BROWSING_CLICK_LISTENER));
        } else if (SafetyCheckProperties.UPDATES_CLICK_LISTENER == propertyKey) {
            fragment.findPreference(UPDATES_KEY)
                    .setOnPreferenceClickListener(
                            (Preference.OnPreferenceClickListener)
                                    model.get(SafetyCheckProperties.UPDATES_CLICK_LISTENER));
        } else if (SafetyCheckProperties.SAFETY_CHECK_BUTTON_CLICK_LISTENER == propertyKey) {
            fragment.getCheckButton()
                    .setOnClickListener(
                            (View.OnClickListener)
                                    model.get(
                                            SafetyCheckProperties
                                                    .SAFETY_CHECK_BUTTON_CLICK_LISTENER));
        } else if (SafetyCheckProperties.LAST_RUN_TIMESTAMP == propertyKey) {
            displayTimestampText(model, fragment);
        } else {
            assert false : "Unhandled property detected in SafetyCheckViewBinder!";
        }
    }

    static void bindPasswordCheckPreferenceModel(
            PropertyModel safetyCheckModel,
            PropertyModel model,
            SafetyCheckSettingsFragment fragment,
            PropertyKey propertyKey,
            String preferenceViewId) {
        if (PasswordsCheckPreferenceProperties.PASSWORDS_STATE == propertyKey) {
            @PasswordsState
            int state = model.get(PasswordsCheckPreferenceProperties.PASSWORDS_STATE);
            fragment.updateElementStatus(
                    preferenceViewId, getStringForPasswords(fragment.getContext(), model, state));
            SafetyCheckElementPreference preference = fragment.findPreference(preferenceViewId);
            preference.setEnabled(true);
            if (state == PasswordsState.UNCHECKED) {
                preference.clearStatusIndicator();
                preference.setEnabled(true);
            } else if (state == PasswordsState.CHECKING) {
                clearTimestampText(fragment);
                preference.showProgressBar();
                preference.setEnabled(false);
            } else {
                displayTimestampText(safetyCheckModel, fragment);
                preference.showStatusIcon(getStatusIconForPasswords(state));
                preference.setEnabled(true);
            }
        } else if (PasswordsCheckPreferenceProperties.PASSWORDS_CLICK_LISTENER == propertyKey) {
            fragment.findPreference(preferenceViewId)
                    .setOnPreferenceClickListener(
                            (Preference.OnPreferenceClickListener)
                                    model.get(
                                            PasswordsCheckPreferenceProperties
                                                    .PASSWORDS_CLICK_LISTENER));
        } else if (PasswordsCheckPreferenceProperties.COMPROMISED_PASSWORDS_COUNT == propertyKey) {
            // Do nothing - this is handled by the PASSWORDS_STATE update.
            return;
        } else if (PasswordsCheckPreferenceProperties.PASSWORDS_TITLE == propertyKey) {
            SafetyCheckElementPreference preference = fragment.findPreference(preferenceViewId);
            preference.setTitle(model.get(PasswordsCheckPreferenceProperties.PASSWORDS_TITLE));
        } else {
            assert false : "Unhandled property detected in SafetyCheckViewBinder!";
        }
    }
}
