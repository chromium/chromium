// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.shared_preferences;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Pair;

import androidx.annotation.GuardedBy;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.Contract;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Layer over android {@link SharedPreferences}.
 *
 * <p>Each instance is bound to one SharedPreferences file. {@link #getInstanceForRegistry} returns
 * the manager for the application-wide default file; {@link #getInstance(String,
 * PreferenceKeyRegistry)} returns a manager for an arbitrarily named file.
 */
@JNINamespace("base::android")
@SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
@NullMarked
public class SharedPreferencesManager {
    // Registries are only used for key checks, which only run with asserts enabled. Without
    // asserts, one manager can serve every registry on the default file, and this field lets
    // getInstanceForRegistry() skip the lock and map lookup on every access.
    private static final @Nullable SharedPreferencesManager sInstance =
            BuildConfig.ENABLE_ASSERTS
                    ? null
                    : new SharedPreferencesManager((PreferenceKeyRegistry) null);

    // Managers keyed by (fileName, registry), where fileName is null for the default file. With
    // asserts, there is one manager per (file, registry) so key checks don't have to consult all
    // registries; without asserts, registry is forced to null.
    @GuardedBy("sInstances")
    private static final Map<
                    Pair<@Nullable String, @Nullable PreferenceKeyRegistry>,
                    SharedPreferencesManager>
            sInstances = new HashMap<>();

    /**
     * The backing SharedPreferences for a named file, or {@code null} when this manager targets the
     * application-wide default file via {@link ContextUtils#getAppSharedPreferences} (see {@link
     * #getSharedPrefs}).
     */
    private final @Nullable SharedPreferences mNamedSharedPrefs;

    private @Nullable PreferenceKeyChecker mKeyChecker;

    /** An Editor that wraps a regular SharedPreferences.Editor while performing key checks. */
    private class CheckingEditor implements SharedPreferences.Editor {
        final SharedPreferences.Editor mWrappedEditor;

        private CheckingEditor(SharedPreferences.Editor wrappedEditor) {
            mWrappedEditor = wrappedEditor;
        }

        @Override
        public SharedPreferences.Editor putString(String key, @Nullable String value) {
            checkIsKeyInUse(key);
            mWrappedEditor.putString(key, value);
            return this;
        }

        @Override
        public SharedPreferences.Editor putStringSet(String key, @Nullable Set<String> values) {
            checkIsKeyInUse(key);
            mWrappedEditor.putStringSet(key, values);
            return this;
        }

        @Override
        public SharedPreferences.Editor putInt(String key, int value) {
            checkIsKeyInUse(key);
            mWrappedEditor.putInt(key, value);
            return this;
        }

        @Override
        public SharedPreferences.Editor putLong(String key, long value) {
            checkIsKeyInUse(key);
            mWrappedEditor.putLong(key, value);
            return this;
        }

        @Override
        public SharedPreferences.Editor putFloat(String key, float value) {
            checkIsKeyInUse(key);
            mWrappedEditor.putFloat(key, value);
            return this;
        }

        @Override
        public SharedPreferences.Editor putBoolean(String key, boolean value) {
            checkIsKeyInUse(key);
            mWrappedEditor.putBoolean(key, value);
            return this;
        }

        @Override
        public SharedPreferences.Editor remove(String key) {
            checkIsKeyInUse(key);
            mWrappedEditor.remove(key);
            return this;
        }

        @Override
        public SharedPreferences.Editor clear() {
            mWrappedEditor.clear();
            return this;
        }

        @Override
        public boolean commit() {
            return mWrappedEditor.commit();
        }

        @Override
        public void apply() {
            mWrappedEditor.apply();
        }
    }

    protected SharedPreferencesManager(@Nullable PreferenceKeyRegistry registry) {
        this(null, registry);
    }

    private SharedPreferencesManager(
            @Nullable String fileName, @Nullable PreferenceKeyRegistry registry) {
        this(
                fileName,
                BuildConfig.ENABLE_ASSERTS ? new StrictPreferenceKeyChecker(registry) : null);
    }

    @VisibleForTesting
    SharedPreferencesManager(@Nullable String fileName, @Nullable PreferenceKeyChecker keyChecker) {
        mNamedSharedPrefs =
                fileName == null
                        ? null
                        : ContextUtils.getApplicationContext()
                                .getSharedPreferences(fileName, Context.MODE_PRIVATE);
        mKeyChecker = keyChecker;
    }

    @VisibleForTesting
    SharedPreferencesManager(PreferenceKeyChecker keyChecker) {
        this(null, keyChecker);
    }

    /**
     * @param registry registry of supported and deprecated preference keys. Should be null when
     *     ENABLE_ASSERTS = false.
     * @return a {@link SharedPreferencesManager} over the application-wide default
     *     SharedPreferences file that operates on keys registered in the passed |registry|
     */
    public static SharedPreferencesManager getInstanceForRegistry(
            @Nullable PreferenceKeyRegistry registry) {
        return getOrCreateInstance(null, registry);
    }

    /**
     * Returns a {@link SharedPreferencesManager} over the SharedPreferences file named |fileName|
     * (as passed to {@link android.content.Context#getSharedPreferences}). The same instance is
     * returned for the same file name (and, with asserts enabled, the same registry).
     *
     * <p>Code under //chrome should not call this directly; use the //chrome entry points that
     * decide which file a key belongs in.
     *
     * @param fileName name of the SharedPreferences file, without the ".xml" suffix.
     * @param registry registry of supported and deprecated preference keys. Should be null when
     *     ENABLE_ASSERTS = false.
     */
    public static SharedPreferencesManager getInstance(
            String fileName, @Nullable PreferenceKeyRegistry registry) {
        assert !fileName.isEmpty();
        @Nullable String normalizedName =
                fileName.equals(ContextUtils.getDefaultSharedPreferencesName()) ? null : fileName;
        return getOrCreateInstance(normalizedName, registry);
    }

    private static SharedPreferencesManager getOrCreateInstance(
            @Nullable String fileName, @Nullable PreferenceKeyRegistry registry) {
        if (fileName == null && !BuildConfig.ENABLE_ASSERTS) {
            return assumeNonNull(sInstance);
        }
        Pair<@Nullable String, @Nullable PreferenceKeyRegistry> key =
                Pair.create(fileName, BuildConfig.ENABLE_ASSERTS ? registry : null);
        // Called from both the UI thread and background threads.
        synchronized (sInstances) {
            SharedPreferencesManager manager = sInstances.get(key);
            if (manager == null) {
                manager = new SharedPreferencesManager(fileName, registry);
                sInstances.put(key, manager);
            }
            return manager;
        }
    }

    /**
     * Drops cached managers for named files, which hold the SharedPreferences of the Context they
     * were created with. Called by Robolectric between tests, since each test gets a new
     * Application.
     */
    public static void resetForTesting() {
        synchronized (sInstances) {
            sInstances.keySet().removeIf(key -> key.first != null);
        }
    }

    /**
     * @return the SharedPreferences this manager reads and writes.
     */
    private SharedPreferences getSharedPrefs() {
        // The default file isn't stored in mNamedSharedPrefs because sInstance is created in a
        // static initializer (possibly before ContextUtils is initialized), and Robolectric
        // replaces the app SharedPreferences for each test, so a cached reference would go stale.
        // ContextUtils.getAppSharedPreferences() is only a static field read.
        return mNamedSharedPrefs != null
                ? mNamedSharedPrefs
                : ContextUtils.getAppSharedPreferences();
    }

    /**
     * @return an Editor to write multiple values to SharedPreferences.
     */
    public SharedPreferences.Editor getEditor() {
        return new CheckingEditor(getSharedPrefs().edit());
    }

    public void disableKeyCheckerForTesting() {
        PreferenceKeyChecker swappedOut = mKeyChecker;
        mKeyChecker = null;
        ResettersForTesting.register(() -> mKeyChecker = swappedOut);
    }

    private void checkIsKeyInUse(String key) {
        if (mKeyChecker != null) {
            mKeyChecker.checkIsKeyInUse(key);
        }
    }

    private void checkIsPrefixInUse(KeyPrefix prefix) {
        if (mKeyChecker != null) {
            mKeyChecker.checkIsPrefixInUse(prefix);
        }
    }

    /**
     * Reads set of String values from preferences.
     *
     * <p>If no value was set for the |key|, returns an unmodifiable empty set.
     *
     * @return unmodifiable Set with the values
     */
    public Set<String> readStringSet(String key) {
        return readStringSet(key, Collections.emptySet());
    }

    /**
     * Reads set of String values from preferences.
     *
     * <p>If no value was set for the |key|, returns an unmodifiable view of |defaultValue|.
     *
     * @return unmodifiable Set with the values
     */
    @Contract("_, !null -> !null")
    public @Nullable Set<String> readStringSet(String key, @Nullable Set<String> defaultValue) {
        checkIsKeyInUse(key);
        Set<String> values = getSharedPrefs().getStringSet(key, defaultValue);
        if (values != null) {
            return Collections.unmodifiableSet(values);
        }
        return null;
    }

    /** Adds a value to string set in shared preferences. */
    public void addToStringSet(String key, String value) {
        checkIsKeyInUse(key);
        // Construct a new set so it can be modified safely. See crbug.com/568369.
        Set<String> values =
                new HashSet<>(getSharedPrefs().getStringSet(key, Collections.emptySet()));
        values.add(value);
        writeStringSetUnchecked(key, values);
    }

    /** Removes value from string set in shared preferences. */
    public void removeFromStringSet(String key, String value) {
        checkIsKeyInUse(key);
        // Construct a new set so it can be modified safely. See crbug.com/568369.
        Set<String> values =
                new HashSet<>(getSharedPrefs().getStringSet(key, Collections.emptySet()));
        if (values.remove(value)) {
            writeStringSetUnchecked(key, values);
        }
    }

    /** Writes string set to shared preferences. */
    public void writeStringSet(String key, Set<String> values) {
        checkIsKeyInUse(key);
        writeStringSetUnchecked(key, values);
    }

    /** Writes string set to shared preferences. */
    private void writeStringSetUnchecked(String key, Set<String> values) {
        SharedPreferences.Editor ed = getSharedPrefs().edit();
        ed.putStringSet(key, values);
        ed.apply();
    }

    /**
     * Writes the given string set to the named shared preference and immediately commit to disk.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     * @return Whether the operation succeeded.
     */
    public boolean writeStringSetSync(String key, Set<String> value) {
        SharedPreferences.Editor ed = getEditor();
        ed.putStringSet(key, value);
        return ed.commit();
    }

    /**
     * Writes the given int value to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    public void writeInt(String key, int value) {
        checkIsKeyInUse(key);
        writeIntUnchecked(key, value);
    }

    private void writeIntUnchecked(String key, int value) {
        SharedPreferences.Editor ed = getSharedPrefs().edit();
        ed.putInt(key, value);
        ed.apply();
    }

    /**
     * Writes the given int value to the named shared preference and immediately commit to disk.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     * @return Whether the operation succeeded.
     */
    public boolean writeIntSync(String key, int value) {
        SharedPreferences.Editor ed = getEditor();
        ed.putInt(key, value);
        return ed.commit();
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
     *
     * @param key The name of the preference to return.
     * @param defaultValue The default value to return if the preference is not set.
     * @return The value of the preference.
     */
    @CalledByNative
    public int readInt(@JniType("std::string") String key, int defaultValue) {
        checkIsKeyInUse(key);
        return getSharedPrefs().getInt(key, defaultValue);
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
     * Sets all int values associated with keys with the given prefix.
     *
     * @param prefix The key prefix for which all values should be set.
     * @param value The value to set the matching keys to.
     */
    public void setIntsWithPrefix(KeyPrefix prefix, int value) {
        SharedPreferences.Editor ed = getEditor();
        for (String key : readAllKeysWithPrefix(prefix)) {
            ed.putInt(key, value);
        }
        ed.apply();
    }

    /**
     * Increments the integer value specified by the given key. If no initial value is present then
     * an initial value of 0 is assumed and incremented, so a new value of 1 is set.
     *
     * @param key The key specifying which integer value to increment.
     * @return The newly incremented value.
     */
    public int incrementInt(String key) {
        int value = readInt(key, 0);
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
        SharedPreferences.Editor ed = getEditor();
        ed.putLong(key, value);
        ed.apply();
    }

    /**
     * Writes the given long value to the named shared preference and immediately commit to disk.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     * @return Whether the operation succeeded.
     */
    public boolean writeLongSync(String key, long value) {
        SharedPreferences.Editor ed = getEditor();
        ed.putLong(key, value);
        return ed.commit();
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
        checkIsKeyInUse(key);
        return getSharedPrefs().getLong(key, defaultValue);
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
     * Sets all long values associated with keys with the given prefix.
     *
     * @param prefix The key prefix for which all values should be set.
     * @param value The value to set the matching keys to.
     */
    public void setLongsWithPrefix(KeyPrefix prefix, long value) {
        SharedPreferences.Editor ed = getEditor();
        for (String key : readAllKeysWithPrefix(prefix)) {
            ed.putLong(key, value);
        }
        ed.apply();
    }

    /**
     * Writes the given float to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    public void writeFloat(String key, float value) {
        SharedPreferences.Editor ed = getEditor();
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
        SharedPreferences.Editor ed = getEditor();
        ed.putFloat(key, value);
        return ed.commit();
    }

    /**
     * Reads the given float value from the named shared preference.
     *
     * @param key The name of the preference to return.
     * @param defaultValue The default value to return if there's no value stored.
     * @return The value of the preference if stored; defaultValue otherwise.
     */
    public float readFloat(String key, float defaultValue) {
        checkIsKeyInUse(key);
        return getSharedPrefs().getFloat(key, defaultValue);
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
     * Sets all float values associated with keys with the given prefix.
     *
     * @param prefix The key prefix for which all values should be set.
     * @param value The value to set the matching keys to.
     */
    public void setFloatsWithPrefix(KeyPrefix prefix, float value) {
        SharedPreferences.Editor ed = getEditor();
        for (String key : readAllKeysWithPrefix(prefix)) {
            ed.putFloat(key, value);
        }
        ed.apply();
    }

    /**
     * Writes the given double value to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    public void writeDouble(String key, double value) {
        SharedPreferences.Editor ed = getEditor();
        // Matches the conversion used in DoubleCachedFeatureParam#writeCacheValueToEditor().
        long ieee754LongValue = Double.doubleToRawLongBits(value);
        ed.putLong(key, ieee754LongValue);
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
        checkIsKeyInUse(key);
        SharedPreferences prefs = getSharedPrefs();
        if (!prefs.contains(key)) {
            return defaultValue;
        }
        long ieee754LongValue = prefs.getLong(key, 0L);
        return Double.longBitsToDouble(ieee754LongValue);
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
     * Sets all double values associated with keys with the given prefix.
     *
     * @param prefix The key prefix for which all values should be set.
     * @param value The value to set the matching keys to.
     */
    public void setDoublesWithPrefix(KeyPrefix prefix, double value) {
        SharedPreferences.Editor ed = getEditor();
        long ieee754LongValue = Double.doubleToRawLongBits(value);
        for (String key : readAllKeysWithPrefix(prefix)) {
            ed.putLong(key, ieee754LongValue);
        }
        ed.apply();
    }

    /**
     * Writes the given boolean to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    public void writeBoolean(String key, boolean value) {
        SharedPreferences.Editor ed = getEditor();
        ed.putBoolean(key, value);
        ed.apply();
    }

    /**
     * Writes the given boolean value to the named shared preference and immediately commit to disk.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     * @return Whether the operation succeeded.
     */
    public boolean writeBooleanSync(String key, boolean value) {
        SharedPreferences.Editor ed = getEditor();
        ed.putBoolean(key, value);
        return ed.commit();
    }

    /**
     * Reads the given boolean value from the named shared preference.
     *
     * @param key The name of the preference to return.
     * @param defaultValue The default value to return if there's no value stored.
     * @return The value of the preference if stored; defaultValue otherwise.
     */
    @CalledByNative
    public boolean readBoolean(@JniType("std::string") String key, boolean defaultValue) {
        checkIsKeyInUse(key);
        return getSharedPrefs().getBoolean(key, defaultValue);
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
     * Sets all boolean values associated with keys with the given prefix.
     *
     * @param prefix The key prefix for which all values should be set.
     * @param value The value to set the matching keys to.
     */
    public void setBooleansWithPrefix(KeyPrefix prefix, boolean value) {
        SharedPreferences.Editor ed = getEditor();
        for (String key : readAllKeysWithPrefix(prefix)) {
            ed.putBoolean(key, value);
        }
        ed.apply();
    }

    /**
     * Writes the given string to the named shared preference.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     */
    @CalledByNative
    public void writeString(
            @JniType("std::string") String key, @Nullable @JniType("std::string") String value) {
        SharedPreferences.Editor ed = getEditor();
        ed.putString(key, value);
        ed.apply();
    }

    /**
     * Writes the given string value to the named shared preference and immediately commit to disk.
     *
     * @param key The name of the preference to modify.
     * @param value The new value for the preference.
     * @return Whether the operation succeeded.
     */
    public boolean writeStringSync(String key, String value) {
        SharedPreferences.Editor ed = getEditor();
        ed.putString(key, value);
        return ed.commit();
    }

    /**
     * Reads the given String value from the named shared preference.
     *
     * @param key The name of the preference to return.
     * @param defaultValue The default value to return if there's no value stored.
     * @return The value of the preference if stored; defaultValue otherwise.
     */
    @CalledByNative
    @Contract("_, !null -> !null")
    public @Nullable @JniType("std::string") String readString(
            @JniType("std::string") String key,
            @JniType("std::string") @Nullable String defaultValue) {
        checkIsKeyInUse(key);
        return getSharedPrefs().getString(key, defaultValue);
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
     * Sets all String values associated with keys with the given prefix.
     *
     * @param prefix The key prefix for which all values should be set.
     * @param value The value to set the matching keys to.
     */
    public void setStringsWithPrefix(KeyPrefix prefix, @Nullable String value) {
        SharedPreferences.Editor ed = getEditor();
        for (String key : readAllKeysWithPrefix(prefix)) {
            ed.putString(key, value);
        }
        ed.apply();
    }

    /**
     * Removes the shared preference entry.
     *
     * @param key The key of the preference to remove.
     */
    @CalledByNative
    public void removeKey(@JniType("std::string") String key) {
        SharedPreferences.Editor ed = getEditor();
        ed.remove(key);
        ed.apply();
    }

    public boolean removeKeySync(String key) {
        SharedPreferences.Editor ed = getEditor();
        ed.remove(key);
        return ed.commit();
    }

    /**
     * Removes all shared preference entries with the given prefix.
     *
     * @param prefix The KeyPrefix for which all entries should be removed.
     */
    public void removeKeysWithPrefix(KeyPrefix prefix) {
        checkIsPrefixInUse(prefix);
        removeKeysWithPrefixInternal(prefix);
    }

    /**
     * Removes all shared preference entries which have a prefix of prefix.createKey(infix + "*").
     *
     * @param prefix The KeyPrefix from which the prefix is constructed.
     * @param infix The String infix from which the prefix is constructed.
     */
    public void removeKeysWithPrefix(KeyPrefix prefix, String infix) {
        checkIsPrefixInUse(prefix);
        removeKeysWithPrefixInternal(new KeyPrefix(prefix.createKey(infix + "*")));
    }

    private void removeKeysWithPrefixInternal(KeyPrefix prefix) {
        SharedPreferences.Editor ed = getEditor();
        Map<String, ?> allPrefs = getSharedPrefs().getAll();
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
    public boolean contains(@JniType("std::string") String key) {
        checkIsKeyInUse(key);
        return getSharedPrefs().contains(key);
    }

    private Set<String> readAllKeysWithPrefix(KeyPrefix prefix) {
        checkIsPrefixInUse(prefix);
        Map<String, ?> allPrefs = getSharedPrefs().getAll();
        Set<String> keysWithPrefix = new HashSet<>();
        for (String key : allPrefs.keySet()) {
            if (prefix.hasGenerated(key)) {
                keysWithPrefix.add(key);
            }
        }
        return keysWithPrefix;
    }

    @SuppressWarnings("unchecked")
    private <T> Map<String, T> readAllWithPrefix(KeyPrefix prefix) {
        checkIsPrefixInUse(prefix);
        Map<String, ?> allPrefs = getSharedPrefs().getAll();
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
