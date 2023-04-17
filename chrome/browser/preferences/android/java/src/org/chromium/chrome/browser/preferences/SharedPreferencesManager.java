// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.build.BuildConfig;

import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Layer over android {@link SharedPreferences}.
 */
@SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
public class SharedPreferencesManager {
    private static class LazyHolder {
        static final SharedPreferencesManager INSTANCE = new SharedPreferencesManager();
    }

    /**
     * @return The SharedPreferencesManager singleton.
     */
    @CalledByNative
    public static SharedPreferencesManager getInstance() {
        return LazyHolder.INSTANCE;
    }

    private BaseChromePreferenceKeyChecker mKeyChecker;

    private SharedPreferencesManager() {
        maybeInitializeChecker();
        // In production builds, use a dummy key checker.
        if (mKeyChecker == null) {
            mKeyChecker = new BaseChromePreferenceKeyChecker();
        }
    }

    @VisibleForTesting
    SharedPreferencesManager(BaseChromePreferenceKeyChecker keyChecker) {
        mKeyChecker = keyChecker;
    }

    private void maybeInitializeChecker() {
        // Create a working key checker, which does not happen in production builds.
        if (BuildConfig.ENABLE_ASSERTS) {
            mKeyChecker = ChromePreferenceKeyChecker.getInstance();
        }
    }

    @VisibleForTesting
    BaseChromePreferenceKeyChecker swapKeyCheckerForTesting(
            BaseChromePreferenceKeyChecker newChecker) {
        BaseChromePreferenceKeyChecker swappedOut = mKeyChecker;
        mKeyChecker = newChecker;
        return swappedOut;
    }

    @VisibleForTesting
    public void disableKeyCheckerForTesting() {
        mKeyChecker = new BaseChromePreferenceKeyChecker();
    }

    /**
     * Observes preference changes.
     */
    public interface Observer {
        /**
         * Notifies when a preference maintained by {@link SharedPreferencesManager} is changed.
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
        ContextUtils.getAppSharedPreferences().registerOnSharedPreferenceChangeListener(listener);
    }

    /**
     * @param observer The {@link Observer} to be removed from observing preference changes.
     */
    public void removeObserver(Observer observer) {
        SharedPreferences.OnSharedPreferenceChangeListener listener = mObservers.get(observer);
        if (listener == null) return;
        ContextUtils.getAppSharedPreferences().unregisterOnSharedPreferenceChangeListener(listener);
    }

    /**
     * Reads set of String values from preferences.
     *
     * If no value was set for the |key|, returns an unmodifiable empty set.
     *
     * @return unmodifiable Set with the values
     */
    public Set<String> readStringSet(String key) {
        return readStringSet(key, Collections.emptySet());
    }

    /**
     * Reads set of String values from preferences.
     *
     * If no value was set for the |key|, returns an unmodifiable view of |defaultValue|.
     *
     * @return unmodifiable Set with the values
     */
    @Nullable
    public Set<String> readStringSet(String key, @Nullable Set<String> defaultValue) {
        mKeyChecker.checkIsKeyInUse(key);
        Set<String> values = ContextUtils.getAppSharedPreferences().getStringSet(key, defaultValue);
        return (values != null) ? Collections.unmodifiableSet(values) : null;
    }

    /**
     * Adds a value to string set in shared preferences.
     */
    public void addToStringSet(String key, String value) {
        mKeyChecker.checkIsKeyInUse(key);
        // Construct a new set so it can be modified safely. See crbug.com/568369.
        Set<String> values = new HashSet<>(
                ContextUtils.getAppSharedPreferences().getStringSet(key, Collections.emptySet()));
        values.add(value);
        writeStringSetUnchecked(key, values);
    }

    /**
     * Removes value from string set in shared preferences.
     */
    public void removeFromStringSet(String key, String value) {
        mKeyChecker.checkIsKeyInUse(key);
        // Construct a new set so it can be modified safely. See crbug.com/568369.
        Set<String> values = new HashSet<>(
                ContextUtils.getAppSharedPreferences().getStringSet(key, Collections.emptySet()));
        if (values.remove(value)) {
            writeStringSetUnchecked(key, values);
        }
    }

    /**
     * Writes string set to shared preferences.
     */
    public void writeStringSet(String key, Set<String> values) {
        mKeyChecker.checkIsKeyInUse(key);
        writeStringSetUnchecked(key, values);
    }

    /**
     * Writes string set to shared preferences.
     */
    private void writeStringSetUnchecked(String key, Set<String> values) {
        Editor editor = ContextUtils.getAppSharedPreferences().edit().putStringSet(key, values);
        editor.apply();
    }

    /**
     * Writes the given string set to the named shared preference and immediately commit to disk.
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     * @return Whether the operation succeeded.
     */
    public boolean writeStringSetSync(String key, Set<String> value) {
        mKeyChecker.checkIsKeyInUse(key);
        Editor editor = ContextUtils.getAppSharedPreferences().edit().putStringSet(key, value);
        return editor.commit();
    }

    /**
     * Writes the given int value to the named shared preference.
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    public void writeInt(String key, int value) {
        mKeyChecker.checkIsKeyInUse(key);
        writeIntUnchecked(key, value);
    }

    private void writeIntUnchecked(String key, int value) {
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        ed.putInt(key, value);
        ed.apply();
    }

    /**
     * Writes the given int value to the named shared preference and immediately commit to disk.
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     * @return Whether the operation succeeded.
     */
    public boolean writeIntSync(String key, int value) {
        mKeyChecker.checkIsKeyInUse(key);
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        ed.putInt(key, value);

        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            return ed.commit();
        }
    }

    /**
     * Writes the given int values to the named shared preferences.
     * @param pairs The key/value pairs to write.
     */
    public void writeInts(Map<String, Integer> pairs) {
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        for (Map.Entry<String, Integer> pair : pairs.entrySet()) {
            mKeyChecker.checkIsKeyInUse(pair.getKey());
            ed.putInt(pair.getKey(), pair.getValue().intValue());
        }
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
        mKeyChecker.checkIsKeyInUse(key);
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return ContextUtils.getAppSharedPreferences().getInt(key, defaultValue);
        }
    }

    /**
     * Reads all int values associated with keys with the given prefix.
     *
     * @param prefix The key prefix for which all values should be returned.
     * @return Map from the keys (in full, not just stem) to Integer values.
     */
    public Map<String, Integer> readIntsWithPrefix(KeyPrefix prefix) {
        return readAllWithPrefix(prefix);
    }

    /**
     * Increments the integer value specified by the given key.  If no initial value is present then
     * an initial value of 0 is assumed and incremented, so a new value of 1 is set.
     * @param key The key specifying which integer value to increment.
     * @return The newly incremented value.
     */
    public int incrementInt(String key) {
        mKeyChecker.checkIsKeyInUse(key);
        int value = ContextUtils.getAppSharedPreferences().getInt(key, 0);
        writeIntUnchecked(key, ++value);
        return value;
    }

    /**
     * Writes the given long to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    public void writeLong(String key, long value) {
        mKeyChecker.checkIsKeyInUse(key);
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        ed.putLong(key, value);
        ed.apply();
    }

    /**
     * Writes the given long value to the named shared preference and immediately commit to disk.
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     * @return Whether the operation succeeded.
     */
    public boolean writeLongSync(String key, long value) {
        mKeyChecker.checkIsKeyInUse(key);
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        ed.putLong(key, value);

        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            return ed.commit();
        }
    }

    /**
     * Writes the given long values to the named shared preferences.
     * @param pairs The key/value pairs to write.
     */
    public void writeLongs(Map<String, Long> pairs) {
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        for (Map.Entry<String, Long> pair : pairs.entrySet()) {
            mKeyChecker.checkIsKeyInUse(pair.getKey());
            ed.putLong(pair.getKey(), pair.getValue().longValue());
        }
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
        mKeyChecker.checkIsKeyInUse(key);
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return ContextUtils.getAppSharedPreferences().getLong(key, defaultValue);
        }
    }

    /**
     * Reads all long values associated with keys with the given prefix.
     *
     * @param prefix The key prefix for which all values should be returned.
     * @return Map from the keys (in full, not just stem) to Long values.
     */
    public Map<String, Long> readLongsWithPrefix(KeyPrefix prefix) {
        return readAllWithPrefix(prefix);
    }

    /**
     * Writes the given float to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    public void writeFloat(String key, float value) {
        mKeyChecker.checkIsKeyInUse(key);
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        ed.putFloat(key, value);
        ed.apply();
    }

    /**
     * Writes the given float value to the named shared preference and immediately commit to disk.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     * @return Whether the operation succeeded.
     */
    public boolean writeFloatSync(String key, float value) {
        mKeyChecker.checkIsKeyInUse(key);
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        ed.putFloat(key, value);

        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            return ed.commit();
        }
    }

    /**
     * Writes the given float values to the named shared preferences.
     * @param pairs The key/value pairs to write.
     */
    public void writeFloats(Map<String, Float> pairs) {
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        for (Map.Entry<String, Float> pair : pairs.entrySet()) {
            mKeyChecker.checkIsKeyInUse(pair.getKey());
            ed.putFloat(pair.getKey(), pair.getValue().floatValue());
        }
        ed.apply();
    }

    /**
     * Reads the given float value from the named shared preference.
     *
     * @param key The name of the preference to return.
     * @param defaultValue The default value to return if there's no value stored.
     * @return The value of the preference if stored; defaultValue otherwise.
     */
    public float readFloat(String key, float defaultValue) {
        mKeyChecker.checkIsKeyInUse(key);
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return ContextUtils.getAppSharedPreferences().getFloat(key, defaultValue);
        }
    }

    /**
     * Reads all float values associated with keys with the given prefix.
     *
     * @param prefix The key prefix for which all values should be returned.
     * @return Map from the keys (in full, not just stem) to Float values.
     */
    public Map<String, Float> readFloatsWithPrefix(KeyPrefix prefix) {
        return readAllWithPrefix(prefix);
    }

    /**
     * Writes the given double value to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    public void writeDouble(String key, double value) {
        mKeyChecker.checkIsKeyInUse(key);
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        long ieee754LongValue = Double.doubleToRawLongBits(value);
        ed.putLong(key, ieee754LongValue);
        ed.apply();
    }

    /**
     * Writes the given double values to the named shared preferences.
     * @param pairs The key/value pairs to write.
     */
    public void writeDoubles(Map<String, Double> pairs) {
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        for (Map.Entry<String, Double> pair : pairs.entrySet()) {
            mKeyChecker.checkIsKeyInUse(pair.getKey());
            long ieee754LongValue = Double.doubleToRawLongBits(pair.getValue());
            ed.putLong(pair.getKey(), ieee754LongValue);
        }
        ed.apply();
    }

    /**
     * Reads the given double value from the named shared preference.
     *
     * @param key The name of the preference to return.
     * @param defaultValue The default value to return if there's no value stored.
     * @return The value of the preference if stored; defaultValue otherwise.
     */
    public Double readDouble(String key, double defaultValue) {
        mKeyChecker.checkIsKeyInUse(key);
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            if (!prefs.contains(key)) {
                return defaultValue;
            }
            long ieee754LongValue = prefs.getLong(key, 0L);
            return Double.longBitsToDouble(ieee754LongValue);
        }
    }

    /**
     * Reads all double values associated with keys with the given prefix.
     *
     * @param prefix The key prefix for which all values should be returned.
     * @return Map from the keys (in full, not just stem) to Double values.
     */
    public Map<String, Double> readDoublesWithPrefix(KeyPrefix prefix) {
        Map<String, Long> longMap = readLongsWithPrefix(prefix);
        Map<String, Double> doubleMap = new HashMap<>();

        for (Map.Entry<String, Long> longEntry : longMap.entrySet()) {
            long ieee754LongValue = longEntry.getValue();
            double doubleValue = Double.longBitsToDouble(ieee754LongValue);
            doubleMap.put(longEntry.getKey(), doubleValue);
        }
        return doubleMap;
    }

    /**
     * Writes the given boolean to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    public void writeBoolean(String key, boolean value) {
        mKeyChecker.checkIsKeyInUse(key);
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        ed.putBoolean(key, value);
        ed.apply();
    }

    /**
     * Writes the given boolean value to the named shared preference and immediately commit to disk.
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     * @return Whether the operation succeeded.
     */
    public boolean writeBooleanSync(String key, boolean value) {
        mKeyChecker.checkIsKeyInUse(key);
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        ed.putBoolean(key, value);

        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            return ed.commit();
        }
    }

    /**
     * Writes the given boolean values to the named shared preferences.
     * @param pairs The key/value pairs to write.
     */
    public void writeBooleans(Map<String, Boolean> pairs) {
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        for (Map.Entry<String, Boolean> pair : pairs.entrySet()) {
            mKeyChecker.checkIsKeyInUse(pair.getKey());
            ed.putBoolean(pair.getKey(), pair.getValue().booleanValue());
        }
        ed.apply();
    }

    /**
     * Reads the given boolean value from the named shared preference.
     *
     * @param key The name of the preference to return.
     * @param defaultValue The default value to return if there's no value stored.
     * @return The value of the preference if stored; defaultValue otherwise.
     */
    @CalledByNative
    public boolean readBoolean(String key, boolean defaultValue) {
        mKeyChecker.checkIsKeyInUse(key);
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return ContextUtils.getAppSharedPreferences().getBoolean(key, defaultValue);
        }
    }

    /**
     * Reads all boolean values associated with keys with the given prefix.
     *
     * @param prefix The key prefix for which all values should be returned.
     * @return Map from the keys (in full, not just stem) to Boolean values.
     */
    public Map<String, Boolean> readBooleansWithPrefix(KeyPrefix prefix) {
        return readAllWithPrefix(prefix);
    }

    /**
     * Writes the given string to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    @CalledByNative
    public void writeString(String key, String value) {
        mKeyChecker.checkIsKeyInUse(key);
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        ed.putString(key, value);
        ed.apply();
    }

    /**
     * Writes the given string value to the named shared preference and immediately commit to disk.
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     * @return Whether the operation succeeded.
     */
    public boolean writeStringSync(String key, String value) {
        mKeyChecker.checkIsKeyInUse(key);
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        ed.putString(key, value);

        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            return ed.commit();
        }
    }

    /**
     * Writes the given String values to the named shared preferences.
     * @param pairs The key/value pairs to write.
     */
    public void writeStrings(Map<String, String> pairs) {
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        for (Map.Entry<String, String> pair : pairs.entrySet()) {
            mKeyChecker.checkIsKeyInUse(pair.getKey());
            ed.putString(pair.getKey(), pair.getValue());
        }
        ed.apply();
    }

    /**
     * Reads the given String value from the named shared preference.
     *
     * @param key The name of the preference to return.
     * @param defaultValue The default value to return if there's no value stored.
     * @return The value of the preference if stored; defaultValue otherwise.
     */
    @Nullable
    @CalledByNative
    public String readString(String key, @Nullable String defaultValue) {
        mKeyChecker.checkIsKeyInUse(key);
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return ContextUtils.getAppSharedPreferences().getString(key, defaultValue);
        }
    }

    /**
     * Reads all String values associated with keys with the given prefix.
     *
     * @param prefix The key prefix for which all values should be returned.
     * @return Map from the keys (in full, not just stem) to String values.
     */
    public Map<String, String> readStringsWithPrefix(KeyPrefix prefix) {
        return readAllWithPrefix(prefix);
    }

    /**
     * Removes the shared preference entry.
     *
     * @param key The key of the preference to remove.
     */
    @CalledByNative
    public void removeKey(String key) {
        mKeyChecker.checkIsKeyInUse(key);
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        ed.remove(key);
        ed.apply();
    }

    public boolean removeKeySync(String key) {
        mKeyChecker.checkIsKeyInUse(key);
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        ed.remove(key);
        return ed.commit();
    }

    /**
     * Removes all shared preference entries with the given prefix.
     *
     * @param prefix The KeyPrefix for which all entries should be removed.
     */
    public void removeKeysWithPrefix(KeyPrefix prefix) {
        mKeyChecker.checkIsPrefixInUse(prefix);
        SharedPreferences.Editor ed = ContextUtils.getAppSharedPreferences().edit();
        Map<String, ?> allPrefs = ContextUtils.getAppSharedPreferences().getAll();
        for (Map.Entry<String, ?> pref : allPrefs.entrySet()) {
            String key = pref.getKey();
            if (prefix.hasGenerated(key)) {
                ed.remove(key);
            }
        }
        ed.apply();
    }

    /**
     * Checks if any value was written associated to a key in shared preferences.
     *
     * @param key The key of the preference to check.
     * @return Whether any value was written for that key.
     */
    @CalledByNative
    public boolean contains(String key) {
        mKeyChecker.checkIsKeyInUse(key);
        return ContextUtils.getAppSharedPreferences().contains(key);
    }

    private <T> Map<String, T> readAllWithPrefix(KeyPrefix prefix) {
        mKeyChecker.checkIsPrefixInUse(prefix);
        Map<String, ?> allPrefs = ContextUtils.getAppSharedPreferences().getAll();
        Map<String, T> allPrefsWithPrefix = new HashMap<>();
        for (Map.Entry<String, ?> pref : allPrefs.entrySet()) {
            String key = pref.getKey();
            if (prefix.hasGenerated(key)) {
                allPrefsWithPrefix.put(key, (T) pref.getValue());
            }
        }
        return allPrefsWithPrefix;
    }
}
