package com.ark.browser.utils;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

import androidx.annotation.NonNull;

import org.chromium.base.ContextUtils;

import java.util.Map;
import java.util.Set;

public class PrefsHelper {

    private static final String DEFAULT_STRING_VALUE = "";
    private static final int DEFAULT_INT_VALUE = -1;
    private static final double DEFAULT_DOUBLE_VALUE = -1d;
    private static final float DEFAULT_FLOAT_VALUE = -1f;
    private static final long DEFAULT_LONG_VALUE = -1L;
    private static final boolean DEFAULT_BOOLEAN_VALUE = false;

    private final SharedPreferences sp;

    private PrefsHelper(SharedPreferences sp) {
        this.sp = sp;
    }

    public static PrefsHelper with() {
        return new PrefsHelper(PreferenceManager.getDefaultSharedPreferences(ContextUtils.getApplicationContext()));
//        return with(ContextUtils.getApplicationContext().getPackageName() + "_preferences");
    }

    public static PrefsHelper with(@NonNull String preferencesName) {
        return new PrefsHelper(ContextUtils.getApplicationContext().getSharedPreferences(
                preferencesName,
                Context.MODE_PRIVATE
        ));
    }

    public static SharedPreferences.Editor edit() {
        return with().editor();
    }

    public static SharedPreferences.Editor edit(@NonNull String preferencesName) {
        return with(preferencesName).editor();
    }

    public SharedPreferences.Editor editor() {
        return sp.edit();
    }

    // String related methods

    /**
     * @param what
     * @return Returns the stored value of 'what'
     */
    public String getString(String what) {
        return getString(what, DEFAULT_STRING_VALUE);
    }

    /**
     * @param what
     * @param defaultString
     * @return Returns the stored value of 'what'
     */
    public String getString(String what, String defaultString) {
        return sp.getString(what, defaultString);
    }

    /**
     * @param where
     * @param what
     */
    public SharedPreferences.Editor putString(String where, String what) {
        return editor().putString(where, what);
    }

    public void applyString(String where, String what) {
        putString(where, what).apply();
    }

    public boolean commitString(String where, String what) {
        return putString(where, what).commit();
    }

    // int related methods

    /**
     * @param what
     * @return Returns the stored value of 'what'
     */
    public int getInt(String what) {
        return getInt(what, DEFAULT_INT_VALUE);
    }

    /**
     * @param what
     * @param defaultInt
     * @return Returns the stored value of 'what'
     */
    public int getInt(String what, int defaultInt) {
        return sp.getInt(what, defaultInt);
    }

    /**
     * @param where
     * @param what
     */
    public SharedPreferences.Editor putInt(String where, int what) {
        return editor().putInt(where, what);
    }

    public void applyInt(String where, int what) {
        putInt(where, what).apply();
    }

    public boolean commitInt(String where, int what) {
        return putInt(where, what).commit();
    }

    // double related methods

    /**
     * @param what
     * @return Returns the stored value of 'what'
     */
    public double getDouble(String what) {
        return getDouble(what, DEFAULT_DOUBLE_VALUE);
    }

    /**
     * @param what
     * @param defaultDouble
     * @return Returns the stored value of 'what'
     */
    public double getDouble(String what, double defaultDouble) {
        if (!contains(what)) {
            return defaultDouble;
        }
        return Double.longBitsToDouble(getLong(what));
    }

    /**
     * @param where
     * @param what
     */
    public SharedPreferences.Editor putDouble(String where, double what) {
        return putLong(where, Double.doubleToRawLongBits(what));
    }

    public void applyDouble(String where, double what) {
        applyLong(where, Double.doubleToRawLongBits(what));
    }

    public boolean commitDouble(String where, double what) {
        return commitLong(where, Double.doubleToRawLongBits(what));
    }

    // float related methods

    /**
     * @param what
     * @return Returns the stored value of 'what'
     */
    public float getFloat(String what) {
        return getFloat(what, DEFAULT_FLOAT_VALUE);
    }

    /**
     * @param what
     * @param defaultFloat
     * @return Returns the stored value of 'what'
     */
    public float getFloat(String what, float defaultFloat) {
        return sp.getFloat(what, defaultFloat);
    }

    /**
     * @param where
     * @param what
     */
    public SharedPreferences.Editor putFloat(String where, float what) {
        return editor().putFloat(where, what);
    }

    public void applyFloat(String where, float what) {
        putFloat(where, what).apply();
    }

    public boolean commitFloat(String where, float what) {
        return putFloat(where, what).commit();
    }

    // long related methods

    /**
     * @param what
     * @return Returns the stored value of 'what'
     */
    public long getLong(String what) {
        return getLong(what, DEFAULT_LONG_VALUE);
    }

    /**
     * @param what
     * @param defaultLong
     * @return Returns the stored value of 'what'
     */
    public long getLong(String what, long defaultLong) {
        return sp.getLong(what, defaultLong);
    }

    /**
     * @param where
     * @param what
     */
    public SharedPreferences.Editor putLong(String where, long what) {
        return editor().putLong(where, what);
    }

    public void applyLong(String where, long what) {
        putLong(where, what).apply();
    }

    public boolean commitLong(String where, long what) {
        return putLong(where, what).commit();
    }

    // boolean related methods

    /**
     * @param what
     * @return Returns the stored value of 'what'
     */
    public boolean getBoolean(String what) {
        return getBoolean(what, DEFAULT_BOOLEAN_VALUE);
    }

    /**
     * @param what
     * @param defaultBoolean
     * @return Returns the stored value of 'what'
     */
    public boolean getBoolean(String what, boolean defaultBoolean) {
        return sp.getBoolean(what, defaultBoolean);
    }

    /**
     * @param where
     * @param what
     */
    public SharedPreferences.Editor putBoolean(String where, boolean what) {
        return editor().putBoolean(where, what);
    }

    public void applyBoolean(String where, boolean what) {
        putBoolean(where, what).apply();
    }

    public boolean commitBoolean(String where, boolean what) {
        return putBoolean(where, what).commit();
    }

    // String set methods

    public Set<String> getStringSet(final String key, final Set<String> defValue) {
        return sp.getStringSet(key, defValue);
    }

    public SharedPreferences.Editor putStringSet(final String key, final Set<String> value) {
        return editor().putStringSet(key, value);
    }

    public void applyStringSet(final String key, final Set<String> value) {
        putStringSet(key, value).apply();
    }

    public boolean commitStringSet(final String key, final Set<String> value) {
        return putStringSet(key, value).commit();
    }

    // end related methods

    /**
     * @param key
     */
    public SharedPreferences.Editor remove(final String key) {
        return sp.edit().remove(key);
    }

    public void applyRemove(final String key) {
        remove(key).apply();
    }

    public boolean commitRemove(final String key) {
        return remove(key).commit();
    }

    /**
     * @param key
     * @return Returns if that key exists
     */
    public boolean contains(final String key) {
        return sp.contains(key);
    }

    public Map<String, ?> getAll() {
        return sp.getAll();
    }

    /**
     * Clear all the preferences
     */
    public void clear() {
        sp.edit().clear().apply();
    }

    public void registerOnChangeListener(SharedPreferences.OnSharedPreferenceChangeListener listener) {
        sp.registerOnSharedPreferenceChangeListener(listener);
    }

    public void unregisterOnChangeListener(SharedPreferences.OnSharedPreferenceChangeListener listener) {
        sp.unregisterOnSharedPreferenceChangeListener(listener);
    }

    public SharedPreferences getSharedPreferences() {
        return sp;
    }
}

