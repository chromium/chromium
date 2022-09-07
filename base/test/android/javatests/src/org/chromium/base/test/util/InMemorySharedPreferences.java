// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.content.SharedPreferences;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Set;

/**
 * An implementation of SharedPreferences that can be used in tests.
 * <p/>
 * It keeps all state in memory, and there is no difference between apply() and commit().
 */
public class InMemorySharedPreferences implements SharedPreferences {

    // Guarded on its own monitor.
    private final Map<String, Object> mData;
    private final List<OnSharedPreferenceChangeListener> mObservers = new ArrayList<>();

    public InMemorySharedPreferences() {
        mData = new HashMap<String, Object>();
    }

    public InMemorySharedPreferences(Map<String, Object> data) {
        mData = data;
    }

    @Override
    public Map<String, ?> getAll() {
        synchronized (mData) {
            return new HashMap<>(mData);
        }
    }

    @Override
    public String getString(String key, String defValue) {
        synchronized (mData) {
            String ret = (String) mData.get(key);
            return ret != null ? ret : defValue;
        }
    }

    @SuppressWarnings("unchecked")
    @Override
    public Set<String> getStringSet(String key, Set<String> defValues) {
        synchronized (mData) {
            Set<String> ret = (Set<String>) mData.get(key);
            return ret != null ? ret : defValues;
        }
    }

    @Override
    public int getInt(String key, int defValue) {
        synchronized (mData) {
            Integer ret = (Integer) mData.get(key);
            return ret != null ? ret : defValue;
        }
    }

    @Override
    public long getLong(String key, long defValue) {
        synchronized (mData) {
            Long ret = (Long) mData.get(key);
            return ret != null ? ret : defValue;
        }
    }

    @Override
    public float getFloat(String key, float defValue) {
        synchronized (mData) {
            Float ret = (Float) mData.get(key);
            return ret != null ? ret : defValue;
        }
    }

    @Override
    public boolean getBoolean(String key, boolean defValue) {
        synchronized (mData) {
            Boolean ret = (Boolean) mData.get(key);
            return ret != null ? ret : defValue;
        }
    }

    @Override
    public boolean contains(String key) {
        synchronized (mData) {
            return mData.containsKey(key);
        }
    }

    @Override
    public SharedPreferences.Editor edit() {
        return new InMemoryEditor();
    }

    @Override
    public void registerOnSharedPreferenceChangeListener(
            SharedPreferences.OnSharedPreferenceChangeListener
                    listener) {
        synchronized (mObservers) {
            mObservers.add(listener);
        }
    }

    @Override
    public void unregisterOnSharedPreferenceChangeListener(
            SharedPreferences.OnSharedPreferenceChangeListener listener) {
        synchronized (mObservers) {
            mObservers.remove(listener);
        }
    }

    private class InMemoryEditor implements SharedPreferences.Editor {

        // All guarded by |mChanges|
        private boolean mClearCalled;
        private final Map<String, Object> mChanges = new HashMap<String, Object>();

        @Override
        public SharedPreferences.Editor putString(String key, String value) {
            synchronized (mChanges) {
                mChanges.put(key, value);
                return this;
            }
        }

        @Override
        public SharedPreferences.Editor putStringSet(String key, Set<String> values) {
            synchronized (mChanges) {
                mChanges.put(key, values);
                return this;
            }
        }

        @Override
        public SharedPreferences.Editor putInt(String key, int value) {
            synchronized (mChanges) {
                mChanges.put(key, value);
                return this;
            }
        }

        @Override
        public SharedPreferences.Editor putLong(String key, long value) {
            synchronized (mChanges) {
                mChanges.put(key, value);
                return this;
            }
        }

        @Override
        public SharedPreferences.Editor putFloat(String key, float value) {
            synchronized (mChanges) {
                mChanges.put(key, value);
                return this;
            }
        }

        @Override
        public SharedPreferences.Editor putBoolean(String key, boolean value) {
            synchronized (mChanges) {
                mChanges.put(key, value);
                return this;
            }
        }

        @Override
        public SharedPreferences.Editor remove(String key) {
            synchronized (mChanges) {
                // Magic value for removes
                mChanges.put(key, this);
                return this;
            }
        }

        @Override
        public SharedPreferences.Editor clear() {
            synchronized (mChanges) {
                mClearCalled = true;
                mChanges.clear();
                return this;
            }
        }

        @Override
        public boolean commit() {
            apply();
            return true;
        }

        @Override
        public void apply() {
            Set<String> changedKeys = new HashSet<>();
            synchronized (mData) {
                synchronized (mChanges) {
                    if (mClearCalled) {
                        changedKeys.addAll(mData.keySet());
                        mData.clear();
                        mClearCalled = false;
                    }
                    for (Map.Entry<String, Object> entry : mChanges.entrySet()) {
                        String key = entry.getKey();
                        Object value = entry.getValue();
                        if (value == this) {
                            if (mData.containsKey(key)) {
                                changedKeys.add(key);
                            }
                            // Special value for removal
                            mData.remove(key);

                        } else {
                            Object oldValue = mData.get(key);
                            if (!Objects.equals(oldValue, value)) {
                                changedKeys.add(key);
                            }

                            mData.put(key, value);
                        }
                    }
                    mChanges.clear();
                }
            }
            synchronized (mObservers) {
                for (OnSharedPreferenceChangeListener observer : mObservers) {
                    for (String key : changedKeys) {
                        observer.onSharedPreferenceChanged(InMemorySharedPreferences.this, key);
                    }
                }
            }
        }
    }

}
