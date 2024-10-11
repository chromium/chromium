// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.cached_flags.BooleanCachedFieldTrialParameter;

/** Feature related utilities for tab groups. */
public class TabGroupFeatureUtils {
    private static final String SKIP_TAB_GROUP_CREATION_DIALOG_PARAM =
            "skip_tab_group_creation_dialog";
    public static final BooleanCachedFieldTrialParameter SKIP_TAB_GROUP_CREATION_DIALOG =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_GROUP_PARITY_ANDROID,
                    SKIP_TAB_GROUP_CREATION_DIALOG_PARAM,
                    true);

    public static final String SHOW_TAB_GROUP_CREATION_DIALOG_SETTING_PARAM =
            "show_tab_group_creation_dialog_setting";
    public static final BooleanCachedFieldTrialParameter SHOW_TAB_GROUP_CREATION_DIALOG_SETTING =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.TAB_GROUP_CREATION_DIALOG_ANDROID,
                    SHOW_TAB_GROUP_CREATION_DIALOG_SETTING_PARAM,
                    false);

    /**
     * Returns whether the group creation dialog should be shown based on the setting switch for
     * auto showing under tab settings. If it is not enabled, return true since that is the default
     * case for all callsites.
     */
    public static boolean shouldShowGroupCreationDialogViaSettingsSwitch() {
        if (SHOW_TAB_GROUP_CREATION_DIALOG_SETTING.getValue()) {
            SharedPreferencesManager prefsManager = ChromeSharedPreferences.getInstance();
            return prefsManager.readBoolean(
                    ChromePreferenceKeys.SHOW_TAB_GROUP_CREATION_DIALOG, true);
        } else {
            return true;
        }
    }

    /** All statics. */
    private TabGroupFeatureUtils() {}
}
