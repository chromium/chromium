// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.night_mode;

import android.support.v7.app.AppCompatDelegate;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.ChromeBaseAppCompatActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/**
 * Holds an instance of {@link NightModeStateProvider} that provides night mode state for the entire
 * application.
 */
public class GlobalNightModeStateProviderHolder {
    private static NightModeStateProvider sInstance;

    /**
     * Created when night mode is not available or not supported.
     */
    private static class DummyNightModeStateProvider implements NightModeStateProvider {
        final boolean mIsNightModeForceEnabled;

        private DummyNightModeStateProvider() {
            mIsNightModeForceEnabled =
                    CommandLine.getInstance().hasSwitch(ChromeSwitches.FORCE_ENABLE_NIGHT_MODE);
            // Always stay in night mode if night mode is force enabled, and always stay in light
            // mode if night mode is not available.
            AppCompatDelegate.setDefaultNightMode(mIsNightModeForceEnabled
                            ? AppCompatDelegate.MODE_NIGHT_YES
                            : AppCompatDelegate.MODE_NIGHT_NO);
        }

        @Override
        public boolean isInNightMode() {
            return mIsNightModeForceEnabled;
        }

        @Override
        public void addObserver(@NonNull Observer observer) {}

        @Override
        public void removeObserver(@NonNull Observer observer) {}
    }

    /**
     * @return The {@link NightModeStateProvider} that maintains the night mode state for the entire
     *         application. Note that UI widgets should always get the
     *         {@link NightModeStateProvider} from the {@link ChromeBaseAppCompatActivity} they are
     *         attached to, because the night mode state can be overridden at the activity level.
     */
    public static NightModeStateProvider getInstance() {
        if (sInstance == null) {
            if (CommandLine.getInstance().hasSwitch(ChromeSwitches.FORCE_ENABLE_NIGHT_MODE)
                    || !NightModeUtils.isNightModeSupported()
                    || !FeatureUtilities.isNightModeAvailable()) {
                sInstance = new DummyNightModeStateProvider();
            } else {
                sInstance = new GlobalNightModeStateController(SystemNightModeMonitor.getInstance(),
                        PowerSavingModeMonitor.getInstance(),
                        SharedPreferencesManager.getInstance());
            }
        }
        return sInstance;
    }

    @VisibleForTesting
    static void resetInstanceForTesting() {
        sInstance = null;
    }
}
