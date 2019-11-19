// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.SharedPreferences;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;

import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Layer over android {@link SharedPreferences}.
 */
public class SharedPreferencesManager {
    private static class LazyHolder {
        static final SharedPreferencesManager INSTANCE = new SharedPreferencesManager();
    }

    /**
     * @return The SharedPreferencesManager singleton.
     */
    public static SharedPreferencesManager getInstance() {
        return LazyHolder.INSTANCE;
    }

    private final SharedPreferences mSharedPreferences;

    private SharedPreferencesManager() {
        this(ContextUtils.getAppSharedPreferences());
    }

    @VisibleForTesting
    SharedPreferencesManager(SharedPreferences sharedPreferences) {
        mSharedPreferences = sharedPreferences;
    }

    /**
     * Observes preference changes.
     */
    public interface Observer {
        /**
         * Notifies when a preference maintained by {@link ChromePreferenceManager} is changed.
         * @param key The key of the preference changed.
         */
        void onPreferenceChanged(String key);
    }

    private final Map<Observer, SharedPreferences.OnSharedPreferenceChangeListener> mObservers =
            new HashMap<>();

    /**
     * @param observer The {@link Observer} to be added for observing preference changes.
     */
    public void addObserver(Observer observer) {
        SharedPreferences.OnSharedPreferenceChangeListener listener =
                (SharedPreferences sharedPreferences, String s) -> observer.onPreferenceChanged(s);
        mObservers.put(observer, listener);
        mSharedPreferences.registerOnSharedPreferenceChangeListener(listener);
    }

    /**
     * @param observer The {@link Observer} to be removed from observing preference changes.
     */
    public void removeObserver(Observer observer) {
        SharedPreferences.OnSharedPreferenceChangeListener listener = mObservers.get(observer);
        if (listener == null) return;
        mSharedPreferences.unregisterOnSharedPreferenceChangeListener(listener);
    }

    /**
     * Reads set of String values from preferences.
     *
     * Note that you must not modify the set instance returned by this call.
     */
    public Set<String> readStringSet(String key) {
        return readStringSet(key, Collections.emptySet());
    }

    /**
     * Reads set of String values from preferences.
     *
     * Note that you must not modify the set instance returned by this call.
     */
    public Set<String> readStringSet(String key, Set<String> defaultValue) {
        return mSharedPreferences.getStringSet(key, defaultValue);
    }

    /**
     * Adds a value to string set in shared preferences.
     */
    public void addToStringSet(String key, String value) {
        Set<String> values = new HashSet<>(readStringSet(key));
        values.add(value);
        writeStringSet(key, values);
    }

    /**
     * Removes value from string set in shared preferences.
     */
    public void removeFromStringSet(String key, String value) {
        Set<String> values = new HashSet<>(readStringSet(key));
        if (values.remove(value)) {
            writeStringSet(key, values);
        }
    }

    /**
     * Writes string set to shared preferences.
     */
    public void writeStringSet(String key, Set<String> values) {
        mSharedPreferences.edit().putStringSet(key, values).apply();
    }

    /**
     * Writes the given int value to the named shared preference.
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    public void writeInt(String key, int value) {
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        ed.putInt(key, value);
        ed.apply();
    }

    /**
     * Reads the given int value from the named shared preference, defaulting to 0 if not found.
     * @param key The name of the preference to return.
     * @return The value of the preference.
     */
    public int readInt(String key) {
        return readInt(key, 0);
    }

    /**
     * Reads the given int value from the named shared preference.
     * @param key The name of the preference to return.
     * @param defaultValue The default value to return if the preference is not set.
     * @return The value of the preference.
     */
    public int readInt(String key, int defaultValue) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return mSharedPreferences.getInt(key, defaultValue);
        }
    }

    /**
     * Increments the integer value specified by the given key.  If no initial value is present then
     * an initial value of 0 is assumed and incremented, so a new value of 1 is set.
     * @param key The key specifying which integer value to increment.
     * @return The newly incremented value.
     */
    public int incrementInt(String key) {
        int value = mSharedPreferences.getInt(key, 0);
        writeInt(key, ++value);
        return value;
    }

    /**
     * Writes the given long to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    public void writeLong(String key, long value) {
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        ed.putLong(key, value);
        ed.apply();
    }

    /**
     * Reads the given long value from the named shared preference.
     *
     * @param key The name of the preference to return.
     * @return The value of the preference if stored; defaultValue otherwise.
     */
    public long readLong(String key) {
        return readLong(key, 0);
    }

    /**
     * Reads the given long value from the named shared preference.
     *
     * @param key The name of the preference to return.
     * @param defaultValue The default value to return if there's no value stored.
     * @return The value of the preference if stored; defaultValue otherwise.
     */
    public long readLong(String key, long defaultValue) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return mSharedPreferences.getLong(key, defaultValue);
        }
    }

    /**
     * Writes the given boolean to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    public void writeBoolean(String key, boolean value) {
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        ed.putBoolean(key, value);
        ed.apply();
    }

    /**
     * Reads the given boolean value from the named shared preference.
     *
     * @param key The name of the preference to return.
     * @param defaultValue The default value to return if there's no value stored.
     * @return The value of the preference if stored; defaultValue otherwise.
     */
    public boolean readBoolean(String key, boolean defaultValue) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return mSharedPreferences.getBoolean(key, defaultValue);
        }
    }

    /**
     * Writes the given string to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    public void writeString(String key, String value) {
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        ed.putString(key, value);
        ed.apply();
    }

    /**
     * Reads the given String value from the named shared preference.
     *
     * @param key The name of the preference to return.
     * @param defaultValue The default value to return if there's no value stored.
     * @return The value of the preference if stored; defaultValue otherwise.
     */
    public String readString(String key, @Nullable String defaultValue) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return mSharedPreferences.getString(key, defaultValue);
        }
    }

    /**
     * Removes the shared preference entry.
     *
     * @param key The key of the preference to remove.
     */
    public void removeKey(String key) {
        SharedPreferences.Editor ed = mSharedPreferences.edit();
        ed.remove(key);
        ed.apply();
    }
}
