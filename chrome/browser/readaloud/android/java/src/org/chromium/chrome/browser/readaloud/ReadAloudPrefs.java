// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.components.prefs.PrefService;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/** Methods for storing and retrieving Read Aloud user settings in prefs. */
public class ReadAloudPrefs {
    private static final String TAG = "ReadAloudSettings";

    private static final String PREF_PATH_PREFIX = "readaloud";
    private static final String VOICES_PATH = PREF_PATH_PREFIX + ".voices";
    private static final String SPEED_PATH = PREF_PATH_PREFIX + ".speed";
    private static final String HIGHLIGHTING_ENABLED_PATH =
            PREF_PATH_PREFIX + ".highlighting_enabled";

    private static final float DEFAULT_SPEED = 1f;
    private static final boolean DEFAULT_HIGHLIGHTING_ENABLED = true;

    private static JSONObject sVoices;

    /**
     * Get the language-to-voiceId map.
     *
     * @param prefs PrefService for profile.
     * @return Read-only voice map. Voice settings should be updated with setVoice() instead.
     */
    public static Map<String, String> getVoices(PrefService prefs) {
        ensureVoicesInit(prefs);
        var languageToVoice = new HashMap<String, String>();
        sVoices.keys()
                .forEachRemaining(
                        (language) -> languageToVoice.put(language, sVoices.optString(language)));

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

        ensureVoicesInit(prefs);
        try {
            sVoices.put(language, voiceId);
            prefs.setString(VOICES_PATH, sVoices.toString());
        } catch (JSONException exception) {
            Log.e(TAG, "Failed to store voice setting: %s", exception.getMessage());
        }
    }

    /**
     * Get the playback speed setting.
     *
     * @param prefs PrefService for profile.
     * @return Playback speed setting.
     */
    public static float getSpeed(PrefService prefs) {
        float speed = DEFAULT_SPEED;
        if (prefs.hasPrefPath(SPEED_PATH)) {
            try {
                speed = Float.parseFloat(prefs.getString(SPEED_PATH));
            } catch (NumberFormatException exception) {
                Log.e(
                        TAG,
                        "Failed to parse speed setting, using default. Details: %s",
                        exception.getMessage());
            }
        }
        return speed;
    }

    /**
     * Set the playback speed setting.
     *
     * @param prefs PrefService for profile.
     * @param speed Playback speed to store.
     */
    public static void setSpeed(PrefService prefs, float speed) {
        prefs.setString(SPEED_PATH, Float.toString(speed));
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
        prefs.setBoolean(HIGHLIGHTING_ENABLED_PATH, enabled);
    }

    private static void ensureVoicesInit(PrefService prefs) {
        if (sVoices != null) {
            return;
        }

        sVoices = new JSONObject();

        if (prefs.hasPrefPath(VOICES_PATH)) {
            try {
                sVoices = new JSONObject(prefs.getString(VOICES_PATH));
            } catch (JSONException exception) {
                Log.e(
                        TAG,
                        "Failed to parse voice settings, using defaults. Details: %s",
                        exception.getMessage());
            }
        }
    }

    static void resetForTesting() {
        sVoices = null;
    }
}
