// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.Nullable;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.cached_flags.BooleanCachedFieldTrialParameter;

/** Feature related utilities for tab groups. */
public class TabGroupFeatureUtils {
    public static final String SHOW_TAB_GROUP_CREATION_DIALOG_SETTING_PARAM =
            "show_tab_group_creation_dialog_setting";
    public static final BooleanCachedFieldTrialParameter SHOW_TAB_GROUP_CREATION_DIALOG_SETTING =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID,
                    SHOW_TAB_GROUP_CREATION_DIALOG_SETTING_PARAM,
                    false);
    private static @Nullable Boolean sTestValueShowTabGroupCreationDialog;

    /**
     * Returns whether the group creation dialog should be shown based on the setting switch for
     * auto showing under tab settings. If it is not enabled, return true since that is the default
     * case for all callsites.
     */
    public static boolean shouldShowGroupCreationDialogViaSettingsSwitch() {
        if (sTestValueShowTabGroupCreationDialog != null) {
            return sTestValueShowTabGroupCreationDialog;
        } else if (SHOW_TAB_GROUP_CREATION_DIALOG_SETTING.getValue()) {
            SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
            return prefsManager.readBoolean(
                    ChromePreferenceKeys.SHOW_TAB_GROUP_CREATION_DIALOG, true);
        } else {
            return true;
        }
    }

    /**
     * Returns whether the group creation dialog will be skipped based on current flags.
     *
     * @param shouldShow Whether the creation dialog should show if TabGroupCreationDialogAndroid is
     *     enabled. Currently it should only show for drag and drop merge and bulk selection editor
     *     merge. It should not show for context menu group creations.
     */
    public static boolean shouldSkipGroupCreationDialog(boolean shouldShow) {
        if (ChromeFeatureList.sTabGroupCreationDialogAndroid.isEnabled()) {
            return !shouldShow;
        } else {
            return true;
        }
    }

    /**
     * Sets the the value to be returned by {@link
     * #shouldShowGroupCreationDialogViaSettingsSwitch()}. When {@link
     * #sTestValueShowTabGroupCreationDialog} is not null, prefsManager will not be called in {@link
     * #shouldShowGroupCreationDialogViaSettingsSwitch()}, allowing for ease in testing different
     * logical branches.
     *
     * @param returnValue The value to be returned by {@link
     *     #shouldShowGroupCreationDialogViaSettingsSwitch()}.
     */
    public static void setsTestValueShowTabGroupCreationDialog(@Nullable Boolean returnValue) {
        sTestValueShowTabGroupCreationDialog = returnValue;
        ResettersForTesting.register(() -> sTestValueShowTabGroupCreationDialog = null);
    }

    /** All statics. */
    private TabGroupFeatureUtils() {}
}
