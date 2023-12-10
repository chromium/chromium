// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.ObserverList;

/** This helper forwards lifecycle events to password manager classes. */
public class PasswordManagerLifecycleHelper {
    private static PasswordManagerLifecycleHelper sInstance;
    private ObserverList<Long> mNativeObservers = new ObserverList<>();

    /**
     * Returns the singleton instance of this class and lazily creates it if that hasn't happened.
     *
     * @return The only {@link PasswordManagerLifecycleHelper}.
     */
    @CalledByNative
    public static PasswordManagerLifecycleHelper getInstance() {
        if (sInstance == null) sInstance = new PasswordManagerLifecycleHelper();
        return sInstance;
    }

    /** Notifies all observers that a foreground session has begun. */
    public void onStartForegroundSession() {
        for (Long observer : mNativeObservers) {
            assert observer != 0;
            PasswordManagerLifecycleHelperJni.get().onForegroundSessionStart(observer);
        }
    }

    @CalledByNative
    @VisibleForTesting
    void registerObserver(long nativePasswordManagerLifecycleHelper) {
        mNativeObservers.addObserver(nativePasswordManagerLifecycleHelper);
    }

    @CalledByNative
    @VisibleForTesting
    void unregisterObserver(long nativePasswordManagerLifecycleHelper) {
        mNativeObservers.removeObserver(nativePasswordManagerLifecycleHelper);
    }

    private PasswordManagerLifecycleHelper() {}

    /** C++ method signatures. */
    @NativeMethods
    interface Natives {
        void onForegroundSessionStart(long nativePasswordManagerLifecycleHelperImpl);
    }
}
