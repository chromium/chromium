// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.util;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;

/** Provide the enterprise information for the current device and profile. */
public abstract class EnterpriseInfo {
    private static final String TAG = "EnterpriseInfo";

    private static EnterpriseInfo sInstance;

    /** A simple tuple to hold onto named fields about the state of ownership. */
    public static class OwnedState {
        public final boolean mDeviceOwned;
        public final boolean mProfileOwned;

        public OwnedState(boolean isDeviceOwned, boolean isProfileOwned) {
            mDeviceOwned = isDeviceOwned;
            mProfileOwned = isProfileOwned;
        }

        @Override
        public boolean equals(Object other) {
            if (this == other) return true;
            if (other == null) return false;
            if (!(other instanceof OwnedState)) return false;

            OwnedState otherOwnedState = (OwnedState) other;

            return this.mDeviceOwned == otherOwnedState.mDeviceOwned
                    && this.mProfileOwned == otherOwnedState.mProfileOwned;
        }
    }

    public static EnterpriseInfo getInstance() {
        ThreadUtils.assertOnUiThread();

        if (sInstance == null) sInstance = new EnterpriseInfoImpl();

        return sInstance;
    }

    /**
     * Returns, via callback, whether the device has a device owner or a profile owner. Guaranteed
     * to not invoke the callback synchronously, instead will be posted to the UI thread, even in
     * tests.
     * @param callback to invoke with results.
     */
    public abstract void getDeviceEnterpriseInfo(Callback<OwnedState> callback);

    /** Records metrics regarding whether the device has a device owner or a profile owner. */
    public abstract void logDeviceEnterpriseInfo();

    /**
     * Overrides the single static {@link EnterpriseInfo}. This instance is shared globally, an if
     * native is initialized in a given test, there will likely be other keyed services crossing the
     * JNI and calling the test instance. The test implementation must uphold the async callback
     * behavior of {@link EnterpriseInfo#getDeviceEnterpriseInfo( Callback )}. Suggested that
     * callers consider using {@link FakeEnterpriseInfo}.
     */
    public static void setInstanceForTest(EnterpriseInfo instance) {
        var oldValue = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldValue);
    }

    @VisibleForTesting
    static void reset() {
        sInstance = null;
    }

    /**
     * Returns, via callback, the owned state for native's AndroidEnterpriseInfo. Guaranteed to not
     * invoke the callback synchronously, instead will be posted to the UI thread, even in tests.
     */
    @CalledByNative
    public static void getManagedStateForNative() {
        Callback<OwnedState> callback =
                (result) -> {
                    Log.i(TAG, "#getManagedStateForNative() " + result);
                    if (result == null) {
                        // Unable to determine the owned state, assume it's not owned.
                        EnterpriseInfoJni.get().updateNativeOwnedState(false, false);
                    } else {
                        EnterpriseInfoJni.get()
                                .updateNativeOwnedState(result.mDeviceOwned, result.mProfileOwned);
                    }
                };

        EnterpriseInfo.getInstance().getDeviceEnterpriseInfo(callback);
    }

    @NativeMethods
    interface Natives {
        void updateNativeOwnedState(boolean hasProfileOwnerApp, boolean hasDeviceOwnerApp);
    }
}
