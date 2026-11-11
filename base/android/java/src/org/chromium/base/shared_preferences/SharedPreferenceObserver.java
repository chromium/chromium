// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.shared_preferences;

import android.content.SharedPreferences;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Set;

/**
 * Subscription returned by {@link SharedPreferencesManager#observe} that runs a callback when one
 * of its observed preference keys changes in the manager's file.
 *
 * <p>Android holds {@link SharedPreferences.OnSharedPreferenceChangeListener}s weakly, so the
 * caller must retain a strong reference to this object (typically in a field) for as long as
 * notifications are desired, and call {@link #destroy()} to unregister.
 */
@NullMarked
public final class SharedPreferenceObserver
        implements SharedPreferences.OnSharedPreferenceChangeListener, Destroyable {
    private final SharedPreferences mPrefs;
    private final Set<String> mKeys;
    private @Nullable Callback<String> mOnKeyChanged;

    SharedPreferenceObserver(
            SharedPreferences prefs, Set<String> keys, Callback<String> onKeyChanged) {
        mPrefs = prefs;
        mKeys = keys;
        mOnKeyChanged = onKeyChanged;
        mPrefs.registerOnSharedPreferenceChangeListener(this);
    }

    /** Stops observing preference changes. Safe to call more than once. */
    @Override
    public void destroy() {
        if (mOnKeyChanged == null) return;
        mOnKeyChanged = null;
        mPrefs.unregisterOnSharedPreferenceChangeListener(this);
    }

    @Override
    public void onSharedPreferenceChanged(SharedPreferences prefs, @Nullable String key) {
        Callback<String> onKeyChanged = mOnKeyChanged;
        if (onKeyChanged != null && key != null && mKeys.contains(key)) {
            onKeyChanged.onResult(key);
        }
    }
}
