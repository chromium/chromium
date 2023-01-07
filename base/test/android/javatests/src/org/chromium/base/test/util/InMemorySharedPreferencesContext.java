// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.content.Context;
import android.content.SharedPreferences;

import java.util.HashMap;
import java.util.Map;

/**
 * ContextWrapper that causes SharedPreferences to not persist to disk.
 */
public class InMemorySharedPreferencesContext extends ApplicationContextWrapper {
    protected final Map<String, SharedPreferences> mSharedPreferences =
            new HashMap<String, SharedPreferences>();

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

    /**
     * Calls .clear() on all SharedPreferences.
     */
    public void clearSharedPreferences() {
        synchronized (mSharedPreferences) {
            // Clear each instance rather than the map in case there are any registered listeners
            // or cached references to them.
            for (SharedPreferences prefs : mSharedPreferences.values()) {
                prefs.edit().clear().apply();
            }
        }
    }
}
