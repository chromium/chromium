// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.CachedMetrics;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.preferences.themes.ThemePreferences;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Records user actions and histograms related to the night mode state.
 */
public class NightModeMetrics {
    private static final CachedMetrics.BooleanHistogramSample BOOLEAN_NIGHT_MODE_STATE =
            new CachedMetrics.BooleanHistogramSample("Android.DarkTheme.EnabledState");

    private static final CachedMetrics
            .EnumeratedHistogramSample ENUMERATED_NIGHT_MODE_ENABLED_REASON =
            new CachedMetrics.EnumeratedHistogramSample(
                    "Android.DarkTheme.EnabledReason", NightModeEnabledReason.NUM_ENTRIES);

    private static final CachedMetrics.EnumeratedHistogramSample ENUMERATED_THEME_PREFERENCE_STATE =
            new CachedMetrics.EnumeratedHistogramSample("Android.DarkTheme.Preference.State",
                    ThemePreferences.ThemeSetting.NUM_ENTRIES);

    /**
     * Different ways that night mode (aka dark theme) can be enabled. This is used for histograms
     * and should therefore be treated as append-only. See DarkThemeEnabledReason in
     * tools/metrics/histograms/enums.xml.
     */
    @IntDef({NightModeEnabledReason.USER_PREFERENCE, NightModeEnabledReason.POWER_SAVE_MODE,
            NightModeEnabledReason.OTHER})
    @Retention(RetentionPolicy.SOURCE)
    private @interface NightModeEnabledReason {
        int USER_PREFERENCE = 0;
        int POWER_SAVE_MODE = 1;
        // TODO(https://crbug.com/941819): Rename this.
        int OTHER = 2;
        int NUM_ENTRIES = 3;
    }

    /**
     * Records the new night mode state in histogram.
     * @param isInNightMode Whether the app is currently in night mode.
     */
    public static void recordNightModeState(boolean isInNightMode) {
        BOOLEAN_NIGHT_MODE_STATE.record(isInNightMode);
    }

    /**
     * Records the reason that night mode is turned on.
     * @param setting The {@link ThemePreferences.ThemeSetting} that the user selects.
     * @param powerSaveModeOn Whether or not power save mode is on.
     */
    public static void recordNightModeEnabledReason(
            @ThemePreferences.ThemeSetting int setting, boolean powerSaveModeOn) {
        ENUMERATED_NIGHT_MODE_ENABLED_REASON.record(
                calculateNightModeEnabledReason(setting, powerSaveModeOn));
    }

    @NightModeEnabledReason
    private static int calculateNightModeEnabledReason(
            @ThemePreferences.ThemeSetting int setting, boolean powerSaveModeOn) {
        if (setting == ThemePreferences.ThemeSetting.DARK) {
            return NightModeEnabledReason.USER_PREFERENCE;
        }
        if (powerSaveModeOn) return NightModeEnabledReason.POWER_SAVE_MODE;
        return NightModeEnabledReason.OTHER;
    }

    /**
     * Records the theme preference state on start up and when theme preference changes.
     * @param setting The new {@link ThemePreferences.ThemeSetting} that the user selects.
     */
    public static void recordThemePreferencesState(@ThemePreferences.ThemeSetting int setting) {
        ENUMERATED_THEME_PREFERENCE_STATE.record(setting);
    }

    /**
     * Records when user changes the theme preferences.
     * @param setting The new {@link ThemePreferences.ThemeSetting} that the user selects.
     */
    public static void recordThemePreferencesChanged(@ThemePreferences.ThemeSetting int setting) {
        switch (setting) {
            case ThemePreferences.ThemeSetting.SYSTEM_DEFAULT:
                RecordUserAction.record("Android.DarkTheme.Preference.SystemDefault");
                break;
            case ThemePreferences.ThemeSetting.LIGHT:
                RecordUserAction.record("Android.DarkTheme.Preference.Light");
                break;
            case ThemePreferences.ThemeSetting.DARK:
                RecordUserAction.record("Android.DarkTheme.Preference.Dark");
                break;
            default:
                assert false : "Theme preferences change should be recorded.";
        }
        recordThemePreferencesState(setting);
    }
}
