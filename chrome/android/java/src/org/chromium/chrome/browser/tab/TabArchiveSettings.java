// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;

/** Class to manage reading/writing preferences related to tab declutter. */
public class TabArchiveSettings {
    static final boolean ARCHIVE_ENABLED_DEFAULT = true;
    static final boolean AUTO_DELETE_ENABLED_DEFAULT = true;

    private final SharedPreferencesManager mPrefsManager;

    /**
     * Constructor.
     *
     * @param prefsManager The {@link SharedPreferencesManager} used to read/write settings.
     */
    public TabArchiveSettings(SharedPreferencesManager prefsManager) {
        mPrefsManager = prefsManager;
    }

    /** Returns whether archive is enabled in settings. */
    public boolean getArchiveEnabled() {
        return mPrefsManager.readBoolean(
                ChromePreferenceKeys.TAB_DECLUTTER_ARCHIVE_ENABLED, ARCHIVE_ENABLED_DEFAULT);
    }

    /** Sets whether archive is enabled in settings. */
    public void setArchiveEnabled(boolean enabled) {
        mPrefsManager.writeBoolean(ChromePreferenceKeys.TAB_DECLUTTER_ARCHIVE_ENABLED, enabled);
    }

    /** Returns the time delta used to determine if a tab is eligible for archive. */
    public int getArchiveTimeDeltaHours() {
        return mPrefsManager.readInt(
                ChromePreferenceKeys.TAB_DECLUTTER_ARCHIVE_TIME_DELTA_HOURS,
                ChromeFeatureList.sAndroidTabDeclutterArchiveTimeDeltaHours.getValue());
    }

    /** Sets the time delta used to determine if a tab is eligible for archive. */
    public void setArchiveTimeDeltaHours(int timeDeltaHours) {
        mPrefsManager.writeInt(
                ChromePreferenceKeys.TAB_DECLUTTER_ARCHIVE_TIME_DELTA_HOURS, timeDeltaHours);
    }

    /** Returns whether auto-deletion of archived tabs is enabled. */
    public boolean isAutoDeleteEnabled() {
        return mPrefsManager.readBoolean(
                ChromePreferenceKeys.TAB_DECLUTTER_AUTO_DELETE_ENABLED,
                AUTO_DELETE_ENABLED_DEFAULT);
    }

    /** Sets whether auto deletion for archived tabs is enabled in settings. */
    public void setAutoDeleteEnabled(boolean enabled) {
        mPrefsManager.writeBoolean(ChromePreferenceKeys.TAB_DECLUTTER_AUTO_DELETE_ENABLED, enabled);
    }

    /**
     * Returns the time delta used to determine if an archived tab is eligible for auto deletion.
     */
    public int getAutoDeleteTimeDeltaHours() {
        return mPrefsManager.readInt(
                ChromePreferenceKeys.TAB_DECLUTTER_AUTO_DELETE_TIME_DELTA_HOURS,
                ChromeFeatureList.sAndroidTabDeclutterAutoDeleteTimeDeltaHours.getValue());
    }

    /** Sets the time delta used to determine if an archived tab is eligible for auto deletion. */
    public void setAutoDeleteTimeDeltaHours(int timeDeltaHours) {
        mPrefsManager.writeInt(
                ChromePreferenceKeys.TAB_DECLUTTER_AUTO_DELETE_TIME_DELTA_HOURS, timeDeltaHours);
    }
}
