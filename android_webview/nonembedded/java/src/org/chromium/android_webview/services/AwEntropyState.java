// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.services;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.metrics.LowEntropySource;

/**
 * Manages the entropy state values for WebView variations.
 *
 * <p>This class handles the generation, storage, and retrieval of the low entropy source used by
 * WebView's services. The Low Entropy Source is stored in non-embedded WebView shared prefs.
 */
@NullMarked
public class AwEntropyState {
    private AwEntropyState() {} // private constructor to prevent instantiation

    private static final String TAG = "AwEntropyState";
    private static final String PREFS_FILE_NAME = "AwEntropyPrefs";
    private static final String WEBVIEW_LOW_ENTROPY_SOURCE = "webview_low_entropy_source";

    /**
     * Reads the low entropy source from SharedPreferences.
     *
     * @return the stored low entropy source, or -1 if not found.
     */
    public static int getLowEntropySource() {
        return ContextUtils.getApplicationContext()
                .getSharedPreferences(PREFS_FILE_NAME, Context.MODE_PRIVATE)
                .getInt(WEBVIEW_LOW_ENTROPY_SOURCE, -1);
    }

    /**
     * Writes the low entropy source to SharedPreferences.
     *
     * @param source The value to write.
     */
    @VisibleForTesting
    public static void setLowEntropySource(int source) {
        ContextUtils.getApplicationContext()
                .getSharedPreferences(PREFS_FILE_NAME, Context.MODE_PRIVATE)
                .edit()
                .putInt(WEBVIEW_LOW_ENTROPY_SOURCE, source)
                .apply();
    }

    /** Clears the low entropy source from SharedPreferences for testing purposes. */
    public static void clearPreferencesForTesting() {
        ContextUtils.getApplicationContext()
                .getSharedPreferences(PREFS_FILE_NAME, Context.MODE_PRIVATE)
                .edit()
                .remove(WEBVIEW_LOW_ENTROPY_SOURCE)
                .apply();
    }

    /**
     * Ensures the low entropy source has been generated and stored. If it doesn't exist, a new
     * value is generated and written to SharedPreferences. This function should only be called from
     * the non-embedded WebView.
     */
    public static void ensureLowEntropySourceInitialized() {
        // Only write the entropy source if it has not been set yet.
        if (getLowEntropySource() == -1) {
            setLowEntropySource(LowEntropySource.generateValue());
        }
    }
}
