// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.content.Context;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.SettingsFragment;

import java.util.Locale;

/** Fragment lifecycle callbacks to record settings metrics. */
@NullMarked
public class SettingsMetricsReporter extends FragmentManager.FragmentLifecycleCallbacks {
    private static final String TAG = "SettingsMetrics";

    private final @Nullable String mMainFragmentTag;

    /**
     * @param mainFragmentTag Optional tag of the main fragment to record metrics for even if it is
     *     not SettingsFragment.
     */
    public SettingsMetricsReporter(@Nullable String mainFragmentTag) {
        mMainFragmentTag = mainFragmentTag;
    }

    @Override
    public void onFragmentAttached(
            FragmentManager fragmentManager, Fragment fragment, Context context) {
        if (!(fragment instanceof SettingsFragment)
                && (mMainFragmentTag == null || !mMainFragmentTag.equals(fragment.getTag()))) {
            return;
        }

        String className = fragment.getClass().getSimpleName();
        RecordHistogram.recordSparseHistogram("Settings.FragmentAttached", className.hashCode());
        // Log hashCode to easily add new class names to enums.xml.
        Log.d(
                TAG,
                String.format(
                        Locale.ENGLISH,
                        "Settings.FragmentAttached: <int value=\"%d\" label=\"%s\"/>",
                        className.hashCode(),
                        className));

        if (!(fragment instanceof SettingsFragment)) {
            RecordHistogram.recordSparseHistogram(
                    "Settings.NonSettingsFragmentAttached", className.hashCode());
            Log.e(
                    TAG,
                    String.format(
                            Locale.ENGLISH, "%s does not implement SettingsFragment", className));
        }
        assert fragment instanceof SettingsFragment
                : className + " does not implement SettingsFragment";
    }
}
