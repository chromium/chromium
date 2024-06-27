// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.content.Context;
import android.content.SharedPreferences;
import android.text.TextUtils;

import androidx.annotation.GuardedBy;

import org.chromium.base.Log;

import java.util.Map;
import java.util.Set;
import java.util.TreeMap;

/** ContextWrapper that causes SharedPreferences to not persist to disk. */
public class InMemorySharedPreferencesContext extends ApplicationContextWrapper {
    private static final String TAG = "InMemorySharedPrefs";

    // Ordered map so that log order is consistent.
    @GuardedBy("mSharedPreferences")
    protected final Map<String, InMemorySharedPreferences> mSharedPreferences = new TreeMap<>();

    public InMemorySharedPreferencesContext(Context base) {
        super(base);
    }

    @Override
    public SharedPreferences getSharedPreferences(String name, int mode) {
        // Pass through multidex prefs to avoid excessive multidex extraction on KitKat.
        if (name.endsWith("multidex.version")) {
            return getBaseContext().getSharedPreferences(name, mode);
        }
        synchronized (mSharedPreferences) {
            if (!mSharedPreferences.containsKey(name)) {
                mSharedPreferences.put(name, new InMemorySharedPreferences());
            }
            return mSharedPreferences.get(name);
        }
    }

    public void resetSharedPreferences() {
        synchronized (mSharedPreferences) {
            int beforeSize = size();
            // Clear each instance rather than the map in case there are any registered listeners
            // or cached references to them.
            for (var entry : mSharedPreferences.entrySet()) {
                String name = entry.getKey();
                InMemorySharedPreferences prefs = entry.getValue();
                Set<String> lines = prefs.toDebugLines();
                prefs.reset();
                if (!lines.isEmpty()) {
                    String joined = TextUtils.join("\n  ", lines);
                    Log.i(TAG, "Cleared all shared prefs from %s:\n  %s", name, joined);
                }
            }
            int afterSize = size();
            Log.i(TAG, "Cleared %s shared prefs via test runner.", beforeSize - afterSize);
        }
    }

    /**
     * Creates a copy of all shared preferences, which can be restored via
     * restoreSharedPreferencesSnapshot().
     */
    public void createSharedPreferencesSnapshot() {
        synchronized (mSharedPreferences) {
            // Clear each instance rather than the map in case there are any registered listeners
            // or cached references to them.
            for (var prefs : mSharedPreferences.values()) {
                prefs.createSnapshot();
            }
            int numFiles = mSharedPreferences.size();
            Log.i(TAG, "Snapshotted shared prefs (%s files, %s total entries)", numFiles, size());
        }
    }

    /**
     * Restores shared preferences to their state the last time createSharedPreferencesSnapshot()
     * was called.
     */
    public void restoreSharedPreferencesSnapshot() {
        synchronized (mSharedPreferences) {
            int beforeSize = size();
            // Clear each instance rather than the map in case there are any registered listeners
            // or cached references to them.
            for (var entry : mSharedPreferences.entrySet()) {
                String name = entry.getKey();
                InMemorySharedPreferences prefs = entry.getValue();
                prefs.restoreSnapshot(name);
            }
            int afterSize = size();
            Log.i(TAG, "Cleared %s shared prefs via test runner.", afterSize - beforeSize);
        }
    }

    @GuardedBy("mSharedPreferences")
    private int size() {
        int ret = 0;
        for (var prefs : mSharedPreferences.values()) {
            ret += prefs.size();
        }
        return ret;
    }
}
