// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.NonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;

/**
 * Bridge between android.webview.WebStorage and native QuotaManager. This object is owned by Java
 * AwBrowserContext and the native side is owned by the native AwBrowserContext.
 */
@Lifetime.Profile
@JNINamespace("android_webview")
public class AwQuotaManagerBridge {
    /**
     * This class represent the callback value of android.webview.WebStorage.getOrigins. The values
     * are optimized for JNI convenience and need to be converted.
     */
    public static class Origins {
        // Origin, usage, and quota data in parallel arrays of same length.
        public final String[] mOrigins;
        public final long[] mUsages;
        public final long[] mQuotas;

        Origins(String[] origins, long[] usages, long[] quotas) {
            mOrigins = origins;
            mUsages = usages;
            mQuotas = quotas;
        }
    }

    // This is not owning. The native object is owned by the native AwBrowserContext.
    private long mNativeAwQuotaManagerBridge;

    public AwQuotaManagerBridge(long nativeAwQuotaManagerBridge) {
        mNativeAwQuotaManagerBridge = nativeAwQuotaManagerBridge;
        AwQuotaManagerBridgeJni.get().init(mNativeAwQuotaManagerBridge, AwQuotaManagerBridge.this);
    }

    /*
     * There are four HTML5 offline storage APIs.
     * 1) Web Storage (ie the localStorage and sessionStorage variables)
     * 2) Web SQL database
     * 3) Indexed Database
     * 4) Filesystem API
     */

    /**
     * Implements WebStorage.deleteAllData(). Clear the storage of all five offline APIs.
     *
     * TODO(boliu): Actually clear Web Storage.
     */
    public void deleteAllData() {
        ThreadUtils.assertOnUiThread();
        AwQuotaManagerBridgeJni.get()
                .deleteAllData(mNativeAwQuotaManagerBridge, AwQuotaManagerBridge.this);
    }

    /** Implements WebStorage.deleteOrigin(). Clear the storage of APIs 2-5 for the given origin. */
    public void deleteOrigin(String origin) {
        ThreadUtils.assertOnUiThread();
        AwQuotaManagerBridgeJni.get()
                .deleteOrigin(mNativeAwQuotaManagerBridge, AwQuotaManagerBridge.this, origin);
    }

    /**
     * Implements WebStorage.getOrigins. Get the per origin usage and quota of APIs 2-5 in
     * aggregate.
     */
    public void getOrigins(@NonNull Callback<Origins> callback) {
        ThreadUtils.assertOnUiThread();
        AwQuotaManagerBridgeJni.get()
                .getOrigins(mNativeAwQuotaManagerBridge, AwQuotaManagerBridge.this, callback);
    }

    /**
     * Implements WebStorage.getQuotaForOrigin. Get the quota of APIs 2-5 in aggregate for given
     * origin.
     */
    public void getQuotaForOrigin(String origin, @NonNull Callback<Long> callback) {
        ThreadUtils.assertOnUiThread();
        AwQuotaManagerBridgeJni.get()
                .getUsageAndQuotaForOrigin(
                        mNativeAwQuotaManagerBridge,
                        AwQuotaManagerBridge.this,
                        origin,
                        callback,
                        true);
    }

    /**
     * Implements WebStorage.getUsageForOrigin. Get the usage of APIs 2-5 in aggregate for given
     * origin.
     */
    public void getUsageForOrigin(String origin, @NonNull Callback<Long> callback) {
        ThreadUtils.assertOnUiThread();
        AwQuotaManagerBridgeJni.get()
                .getUsageAndQuotaForOrigin(
                        mNativeAwQuotaManagerBridge,
                        AwQuotaManagerBridge.this,
                        origin,
                        callback,
                        false);
    }

    @CalledByNative
    private void onGetOriginsCallback(
            Callback<Origins> callback, String[] origin, long[] usages, long[] quotas) {
        callback.onResult(new Origins(origin, usages, quotas));
    }

    @NativeMethods
    interface Natives {
        void init(long nativeAwQuotaManagerBridge, AwQuotaManagerBridge caller);

        void deleteAllData(long nativeAwQuotaManagerBridge, AwQuotaManagerBridge caller);

        void deleteOrigin(
                long nativeAwQuotaManagerBridge, AwQuotaManagerBridge caller, String origin);

        void getOrigins(
                long nativeAwQuotaManagerBridge,
                AwQuotaManagerBridge caller,
                Callback<Origins> callback);

        void getUsageAndQuotaForOrigin(
                long nativeAwQuotaManagerBridge,
                AwQuotaManagerBridge caller,
                String origin,
                Callback<Long> callback,
                boolean isQuota);
    }
}
