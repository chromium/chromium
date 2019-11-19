// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences;

import android.util.SparseArray;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

/**
 * This class is the Java implementation of native PrefChangeRegistrar. It receives notification for
 * changes of one or more preferences from native PrefService.
 *
 * Note that {@link #destroy} should be called to destroy the native PrefChangeRegistrar.
 */
public class PrefChangeRegistrar {
    /**
     * Interface for callback when registered preference is changed.
     */
    public interface PrefObserver { void onPreferenceChange(); }

    /** Mapping preference index and corresponding observer. **/
    private final SparseArray<PrefObserver> mObservers = new SparseArray<>();

    /** Native pointer for PrefChangeRegistrarAndroid. **/
    private long mNativeRegistrar;

    /**
     * Initialize native PrefChangeRegistrar.
     */
    public PrefChangeRegistrar() {
        mNativeRegistrar = PrefChangeRegistrarJni.get().init(PrefChangeRegistrar.this);
    }

    /**
     * Add an observer to be notified of changes to the specified preference.
     * @param preference The preference to be observed.
     * @param observer The observer to be notified.
     */
    public void addObserver(@Pref int preference, PrefObserver observer) {
        assert mObservers.get(preference)
                == null : "Only one observer should be added to each preference.";
        mObservers.put(preference, observer);
        PrefChangeRegistrarJni.get().add(mNativeRegistrar, PrefChangeRegistrar.this, preference);
    }

    /**
     * Remove an observer for the specified preference if it has previously been added.
     * @param preference The specified preference.
     */
    public void removeObserver(@Pref int preference) {
        PrefObserver observer = mObservers.get(preference);
        if (observer == null) return;
        mObservers.remove(preference);
        PrefChangeRegistrarJni.get().remove(mNativeRegistrar, PrefChangeRegistrar.this, preference);
    }

    /**
     * Destroy native PrefChangeRegistrar.
     */
    public void destroy() {
        if (mNativeRegistrar != 0) {
            PrefChangeRegistrarJni.get().destroy(mNativeRegistrar, PrefChangeRegistrar.this);
        }
        mNativeRegistrar = 0;
    }

    @CalledByNative
    private void onPreferenceChange(int preference) {
        assert mObservers.get(preference)
                != null : "Notification from unregistered preference changes.";
        mObservers.get(preference).onPreferenceChange();
    }

    @NativeMethods
    interface Natives {
        long init(PrefChangeRegistrar caller);
        void add(long nativePrefChangeRegistrarAndroid, PrefChangeRegistrar caller, int preference);
        void remove(
                long nativePrefChangeRegistrarAndroid, PrefChangeRegistrar caller, int preference);
        void destroy(long nativePrefChangeRegistrarAndroid, PrefChangeRegistrar caller);
    }
}
