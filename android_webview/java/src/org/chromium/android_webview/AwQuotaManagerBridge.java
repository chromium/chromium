// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.url.GURL;

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
    private final long mNativeAwQuotaManagerBridge;

    public AwQuotaManagerBridge(long nativeAwQuotaManagerBridge) {
        mNativeAwQuotaManagerBridge = nativeAwQuotaManagerBridge;
    }

    /*
     * There are four HTML5 offline storage APIs.
     * 1) Web Storage (ie the localStorage and sessionStorage variables)
     * 2) Web SQL database
     * 3) Indexed Database
     * 4) Filesystem API
     */

    /**
     * Implements WebStorage.deleteAllData(). Clear the storage of all four offline APIs.
     *
     * <p>This implementation is used by the Android Framework API {@link
     * android.webkit.WebStorage#deleteAllData()}.
     */
    public void deleteAllDataFramework() {
        ThreadUtils.assertOnUiThread();
        AwQuotaManagerBridgeJni.get().deleteAllDataFramework(mNativeAwQuotaManagerBridge);
    }

    /**
     * Implements WebStorage.deleteOrigin(). Clear the storage of APIs 2-4 for the given origin.
     *
     * <p>This implementation is used by the Android Framework API {@link
     * android.webkit.WebStorage#deleteOrigin(String)}.
     *
     * @param origin The origin to perform deletion for.
     */
    public void deleteOriginFramework(String origin) {
        ThreadUtils.assertOnUiThread();
        AwQuotaManagerBridgeJni.get().deleteOriginFramework(mNativeAwQuotaManagerBridge, origin);
    }

    /**
     * Delete all local storage held by WebView.
     *
     * @param resultCallback Callback executed when the deletion completes.
     */
    public void deleteBrowsingData(@NonNull Callback<Boolean> resultCallback) {
        ThreadUtils.assertOnUiThread();
        AwQuotaManagerBridgeJni.get()
                .deleteBrowsingData(mNativeAwQuotaManagerBridge, resultCallback);
    }

    /**
     * Delete all data for the eTLD+1 of the passed-in hostname.
     *
     * <p>Any subdomain in the {@code domain} parameter will be ignored.
     *
     * @param urlOrDomain Host to delete data for.
     * @param resultCallback Callback executed when the deletion completes.
     * @return The actual hostname used for deletion. This may differ from the {@code domain}
     *     parameter if the former contained a subdomain.
     */
    @NonNull
    public String deleteBrowsingDataForSite(
            @NonNull String urlOrDomain, @NonNull Callback<Boolean> resultCallback) {
        ThreadUtils.assertOnUiThread();
        // Attempt to parse the hostname as part of a HTTP url.
        String domain = getDomainName(urlOrDomain);
        if (domain == null) {
            throw new IllegalArgumentException("Invalid domain name: " + urlOrDomain);
        }
        return AwQuotaManagerBridgeJni.get()
                .deleteBrowsingDataForSite(mNativeAwQuotaManagerBridge, domain, resultCallback);
    }

    /**
     * Attempt to extract the domain name from the passed-in URL or domain name.
     *
     * <p>This method will attempt to parse the value as a URL directly, or as a domain name by
     * prefixing it with 'http://'.
     *
     * <p>It will return {@code null} if unable to parse the value as a URL using either of those
     * methods.
     *
     * @param urlOrDomain Input value to extract domain name from
     * @return domain name from the input value, or {@code null} if unable to extract a domain name.
     */
    @Nullable
    @VisibleForTesting
    public static String getDomainName(@NonNull String urlOrDomain) {
        // Attempt to just parse the passed-in value as a URL directly.
        GURL parsed = new GURL(urlOrDomain);
        if (parsed.isValid() && !parsed.getHost().isEmpty()) {
            return parsed.getHost();
        }

        parsed = new GURL("http://" + urlOrDomain);
        if (parsed.isValid() && !parsed.getHost().isEmpty()) {
            return parsed.getHost();
        }
        return null;
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
        void deleteAllDataFramework(long nativeAwQuotaManagerBridge);

        void deleteOriginFramework(long nativeAwQuotaManagerBridge, String origin);

        void deleteBrowsingData(long nativeAwQuotaManagerBridge, Callback<Boolean> callback);

        @NonNull
        @JniType("std::string")
        String deleteBrowsingDataForSite(
                long nativeAwQuotaManagerBridge,
                @JniType("std::string") String domain,
                Callback<Boolean> callback);

        void getOrigins(
                long nativeAwQuotaManagerBridge,
                AwQuotaManagerBridge caller,
                Callback<Origins> callback);

        void getUsageAndQuotaForOrigin(
                long nativeAwQuotaManagerBridge,
                String origin,
                Callback<Long> callback,
                boolean isQuota);
    }
}
