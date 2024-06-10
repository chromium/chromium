// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.content.ComponentCallbacks;
import android.content.ContentResolver;
import android.content.Context;
import android.test.mock.MockContentResolver;
import android.test.mock.MockContext;

import java.util.HashMap;
import java.util.Map;

/**
 * ContextWrapper that adds functionality for SharedPreferences and a way to set and retrieve flags.
 */
public class AdvancedMockContext extends InMemorySharedPreferencesContext {
    private final MockContentResolver mMockContentResolver = new MockContentResolver();

    private final Map<String, Boolean> mFlags = new HashMap<String, Boolean>();

    public AdvancedMockContext(Context base) {
        super(base);
    }

    public AdvancedMockContext() {
        super(new MockContext());
    }

    @Override
    public String getPackageName() {
        return getBaseContext().getPackageName();
    }

    @Override
    public Context getApplicationContext() {
        return this;
    }

    @Override
    public ContentResolver getContentResolver() {
        return mMockContentResolver;
    }

    public MockContentResolver getMockContentResolver() {
        return mMockContentResolver;
    }

    @Override
    public void registerComponentCallbacks(ComponentCallbacks callback) {
        getBaseContext().registerComponentCallbacks(callback);
    }

    @Override
    public void unregisterComponentCallbacks(ComponentCallbacks callback) {
        getBaseContext().unregisterComponentCallbacks(callback);
    }

    public void setFlag(String key) {
        mFlags.put(key, true);
    }

    public void clearFlag(String key) {
        mFlags.remove(key);
    }

    public boolean isFlagSet(String key) {
        return mFlags.containsKey(key) && mFlags.get(key);
    }
}
