// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.components.prefs.PrefService;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/** Methods for storing and retrieving Read Aloud user settings in prefs. */
@JNINamespace("readaloud")
public class ReadAloudPrefs {
    private static final String TAG = "ReadAloudSettings";

    // Keep these names in sync with those in //chrome/common/pref_names.h.
    private static final String PREF_PATH_PREFIX = "readaloud";
    private static final String SPEED_PATH = PREF_PATH_PREFIX + ".speed";
    static final String HIGHLIGHTING_ENABLED_PATH = PREF_PATH_PREFIX + ".highlighting_enabled";

    private static final float DEFAULT_SPEED = 1f;
    private static final boolean DEFAULT_HIGHLIGHTING_ENABLED = true;

    private ReadAloudPrefs() {}

    /**
     * Get the language-to-voiceId map.
     *
     * @param prefs PrefService for profile.
     * @return Read-only voice map. Voice settings should be updated with setVoice() instead.
     */
    public static Map<String, String> getVoices(PrefService prefs) {
        var languageToVoice = new HashMap<String, String>();
        ReadAloudPrefsJni.get().getVoices(prefs, languageToVoice);
        return Collections.unmodifiableMap(languageToVoice);
    }

    /**
     * Store the voice ID to use for the given language.
     *
     * @param prefs PrefService for profile.
     * @param language Language code.
     * @param voiceId ID of the voice to use for this language.
     */
    public static void setVoice(PrefService prefs, String language, String voiceId) {
        if (language == null || language.isEmpty() || voiceId == null || voiceId.isEmpty()) {
            return;
        }
        ReadAloudPrefsJni.get().setVoice(prefs, language, voiceId);
    }

    /**
     * Get the playback speed setting.
     *
     * @param prefs PrefService for profile.
     * @return Playback speed setting.
     */
    public static float getSpeed(PrefService prefs) {
        return (float)
                (prefs.hasPrefPath(SPEED_PATH) ? prefs.getDouble(SPEED_PATH) : DEFAULT_SPEED);
    }

    /**
     * Set the playback speed setting.
     *
     * @param prefs PrefService for profile.
     * @param speed Playback speed to store.
     */
    public static void setSpeed(PrefService prefs, float speed) {
        if (speed != getSpeed(prefs)) {
            ReadAloudMetrics.recordSpeedChange(speed);
        }
        prefs.setDouble(SPEED_PATH, (double) speed);
    }

    /**
     * Get the highlighting enabled setting.
     *
     * @param prefs PrefService for profile.
     * @return Highlighting enabled setting.
     */
    public static boolean isHighlightingEnabled(PrefService prefs) {
        return prefs.hasPrefPath(HIGHLIGHTING_ENABLED_PATH)
                ? prefs.getBoolean(HIGHLIGHTING_ENABLED_PATH)
                : DEFAULT_HIGHLIGHTING_ENABLED;
    }

    /**
     * Set the highlighting enabled setting.
     *
     * @param prefs PrefService for profile.
     * @param enabled True if highlighting should happen if available, false otherwise.
     */
    public static void setHighlightingEnabled(PrefService prefs, boolean enabled) {
        // setHighlightingEnabled is called on every session start. This checks that the value has
        // changed to prevent redundant metric recording at session start.
        if (enabled != isHighlightingEnabled(prefs)) {
            ReadAloudMetrics.recordHighlightingEnabledChanged(enabled);
            prefs.setBoolean(HIGHLIGHTING_ENABLED_PATH, enabled);
        }
    }

    /**
     * Return the client ID for reliability logging.
     *
     * @param prefs PrefService where the random salt used to generate the ID is stored.
     */
    public static long getReliabilityLoggingId(PrefService prefs) {
        if (prefs == null) return 0L;
        return ReadAloudPrefsJni.get()
                .getReliabilityLoggingId(prefs, ReadAloudFeatures.getMetricsId());
    }

    @NativeMethods
    public interface Natives {
        void getVoices(PrefService prefService, Map<String, String> output);

        void setVoice(PrefService prefService, String language, String voiceId);

        long getReliabilityLoggingId(PrefService prefService, String metricsId);
    }
}
