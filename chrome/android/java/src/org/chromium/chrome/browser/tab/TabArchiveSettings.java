// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.SharedPreferences;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;

import java.util.Set;
import java.util.concurrent.TimeUnit;

/** Class to manage reading/writing preferences related to tab declutter. */
public class TabArchiveSettings {
    public interface Observer {
        // Called when a setting was changed.
        void onSettingChanged();
    }

    @VisibleForTesting static final boolean ARCHIVE_ENABLED_DEFAULT = true;
    @VisibleForTesting static final boolean AUTO_DELETE_ENABLED_DEFAULT = true;
    @VisibleForTesting static final boolean DIALOG_IPH_DEFAULT = true;
    private static final Set<String> PREF_KEYS_FOR_NOTIFICATIONS =
            Set.of(
                    ChromePreferenceKeys.TAB_DECLUTTER_ARCHIVE_ENABLED,
                    ChromePreferenceKeys.TAB_DECLUTTER_ARCHIVE_TIME_DELTA_HOURS,
                    ChromePreferenceKeys.TAB_DECLUTTER_AUTO_DELETE_ENABLED,
                    ChromePreferenceKeys.TAB_DECLUTTER_AUTO_DELETE_TIME_DELTA_HOURS);

    private SharedPreferences.OnSharedPreferenceChangeListener mPrefsListener =
            new SharedPreferences.OnSharedPreferenceChangeListener() {
                @Override
                public void onSharedPreferenceChanged(SharedPreferences sharedPrefs, String key) {

                    PostTask.postTask(TaskTraits.UI_DEFAULT, () -> maybeNotifyObservers(key));
                }
            };

    private final SharedPreferencesManager mPrefsManager;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    /**
     * Constructor.
     *
     * @param prefsManager The {@link SharedPreferencesManager} used to read/write settings.
     */
    public TabArchiveSettings(SharedPreferencesManager prefsManager) {
        mPrefsManager = prefsManager;
        ContextUtils.getAppSharedPreferences()
                .registerOnSharedPreferenceChangeListener(mPrefsListener);
    }

    /** Destroys the object, unregistering observers. */
    public void destroy() {
        ContextUtils.getAppSharedPreferences()
                .unregisterOnSharedPreferenceChangeListener(mPrefsListener);
    }

    /** Adds the given observer to the list. */
    public void addObserver(Observer obs) {
        mObservers.addObserver(obs);
    }

    /** Removes the given observer from the list. */
    public void removeObserver(Observer obs) {
        mObservers.removeObserver(obs);
    }

    /** Returns whether archive is enabled in settings. */
    public boolean getArchiveEnabled() {
        // Turn off the archive feature by default for tests since we can't control when tabs
        // are created, and tabs disappearing from tests is very unexpected. For archive tests,
        // this will need to be turned on manually.
        return mPrefsManager.readBoolean(
                ChromePreferenceKeys.TAB_DECLUTTER_ARCHIVE_ENABLED,
                BuildConfig.IS_FOR_TEST ? false : ARCHIVE_ENABLED_DEFAULT);
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

    /** Similar to above, but the return value is in days. */
    public int getArchiveTimeDeltaDays() {
        return (int)
                TimeUnit.HOURS.toDays(
                        mPrefsManager.readInt(
                                ChromePreferenceKeys.TAB_DECLUTTER_ARCHIVE_TIME_DELTA_HOURS,
                                ChromeFeatureList.sAndroidTabDeclutterArchiveTimeDeltaHours
                                        .getValue()));
    }

    /** Sets the time delta used to determine if a tab is eligible for archive. */
    public void setArchiveTimeDeltaHours(int timeDeltaHours) {
        mPrefsManager.writeInt(
                ChromePreferenceKeys.TAB_DECLUTTER_ARCHIVE_TIME_DELTA_HOURS, timeDeltaHours);
    }

    /** Sets the time delta in daysused to determine if a tab is eligible for archive. */
    public void setArchiveTimeDeltaDays(int timeDeltaHours) {
        setArchiveTimeDeltaHours((int) TimeUnit.DAYS.toHours(timeDeltaHours));
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

    /** Similar to above, but the return value is in days. */
    public int getAutoDeleteTimeDeltaDays() {
        return (int)
                TimeUnit.HOURS.toDays(
                        mPrefsManager.readInt(
                                ChromePreferenceKeys.TAB_DECLUTTER_AUTO_DELETE_TIME_DELTA_HOURS,
                                ChromeFeatureList.sAndroidTabDeclutterAutoDeleteTimeDeltaHours
                                        .getValue()));
    }

    /** Sets the time delta used to determine if an archived tab is eligible for auto deletion. */
    public void setAutoDeleteTimeDeltaHours(int timeDeltaHours) {
        mPrefsManager.writeInt(
                ChromePreferenceKeys.TAB_DECLUTTER_AUTO_DELETE_TIME_DELTA_HOURS, timeDeltaHours);
    }

    /** Returns the interval to perform declutter in hours. */
    public int getDeclutterIntervalTimeDeltaHours() {
        return ChromeFeatureList.sAndroidTabDeclutterIntervalTimeDeltaHours.getValue();
    }

    /** Returns whether the dialog iph should be shown. */
    public boolean shouldShowDialogIph() {
        return mPrefsManager.readBoolean(
                ChromePreferenceKeys.TAB_DECLUTTER_DIALOG_IPH, DIALOG_IPH_DEFAULT);
    }

    /** Sets whether the dialog iph should be shown. */
    public void setShouldShowDialogIph(boolean shouldShow) {
        mPrefsManager.writeBoolean(ChromePreferenceKeys.TAB_DECLUTTER_DIALOG_IPH, shouldShow);
    }

    // Private methods.

    private void maybeNotifyObservers(String key) {
        if (!PREF_KEYS_FOR_NOTIFICATIONS.contains(key)) return;

        for (Observer obs : mObservers) {
            obs.onSettingChanged();
        }
    }

    // Testing-specific methods.

    public void resetSettingsForTesting() {
        mPrefsManager.removeKey(ChromePreferenceKeys.TAB_DECLUTTER_ARCHIVE_ENABLED);
        mPrefsManager.removeKey(ChromePreferenceKeys.TAB_DECLUTTER_ARCHIVE_TIME_DELTA_HOURS);
        mPrefsManager.removeKey(ChromePreferenceKeys.TAB_DECLUTTER_AUTO_DELETE_ENABLED);
        mPrefsManager.removeKey(ChromePreferenceKeys.TAB_DECLUTTER_AUTO_DELETE_TIME_DELTA_HOURS);
        mPrefsManager.removeKey(ChromePreferenceKeys.TAB_DECLUTTER_DIALOG_IPH);
    }
}
