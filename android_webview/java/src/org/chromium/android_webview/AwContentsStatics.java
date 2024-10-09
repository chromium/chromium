// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.net.Uri;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.android_webview.common.Flag;
import org.chromium.android_webview.common.FlagOverrideHelper;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.ProductionSupportedFlagList;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingSafeModeAction;
import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Implementations of various static methods, and also a home for static
 * data structures that are meant to be shared between all webviews.
 */
@Lifetime.Singleton
@JNINamespace("android_webview")
public class AwContentsStatics {
    private static ClientCertLookupTable sClientCertLookupTable;

    private static String sUnreachableWebDataUrl;

    private static boolean sRecordFullDocument;

    /** Return the client certificate lookup table. */
    public static ClientCertLookupTable getClientCertLookupTable() {
        ThreadUtils.assertOnUiThread();
        if (sClientCertLookupTable == null) {
            sClientCertLookupTable = new ClientCertLookupTable();
        }
        return sClientCertLookupTable;
    }

    /** Clear client cert lookup table. Should only be called from UI thread. */
    public static void clearClientCertPreferences(Runnable callback) {
        ThreadUtils.assertOnUiThread();
        getClientCertLookupTable().clear();
        AwContentsStaticsJni.get().clearClientCertPreferences(callback);
    }

    @CalledByNative
    private static void clientCertificatesCleared(Runnable callback) {
        if (callback == null) return;
        callback.run();
    }

    public static String getUnreachableWebDataUrl() {
        // Note that this method may be called from both IO and UI threads,
        // but as it only retrieves a value of a constant from native, even if
        // two calls will be running at the same time, this should not cause
        // any harm.
        if (sUnreachableWebDataUrl == null) {
            sUnreachableWebDataUrl = AwContentsStaticsJni.get().getUnreachableWebDataUrl();
        }
        return sUnreachableWebDataUrl;
    }

    public static void setRecordFullDocument(boolean recordFullDocument) {
        sRecordFullDocument = recordFullDocument;
    }

    /* package */ static boolean getRecordFullDocument() {
        return sRecordFullDocument;
    }

    public static String getProductVersion() {
        return AwContentsStaticsJni.get().getProductVersion();
    }

    @CalledByNative
    private static void safeBrowsingAllowlistAssigned(Callback<Boolean> callback, boolean success) {
        if (callback == null) return;
        callback.onResult(success);
    }

    public static void setSafeBrowsingAllowlist(List<String> urls, Callback<Boolean> callback) {
        String[] urlArray = urls.toArray(new String[urls.size()]);
        if (callback == null) {
            callback = CallbackUtils.emptyCallback();
        }
        AwContentsStaticsJni.get().setSafeBrowsingAllowlist(urlArray, callback);
    }

    @SuppressWarnings("NoContextGetApplicationContext")
    public static void initSafeBrowsing(Context context, final Callback<Boolean> callback) {
        // Wrap the callback to make sure we always invoke it on the UI thread, as guaranteed by the
        // API.
        Callback<Boolean> wrapperCallback =
                b -> {
                    if (callback != null) {
                        PostTask.runOrPostTask(TaskTraits.UI_DEFAULT, callback.bind(b));
                    }
                };

        if (AwSafeBrowsingSafeModeAction.isSafeBrowsingDisabled()) {
            wrapperCallback.onResult(PlatformServiceBridge.getInstance().canUseGms());
            return;
        }

        PlatformServiceBridge.getInstance()
                .warmUpSafeBrowsing(context.getApplicationContext(), wrapperCallback);
    }

    public static Uri getSafeBrowsingPrivacyPolicyUrl() {
        return Uri.parse(AwContentsStaticsJni.get().getSafeBrowsingPrivacyPolicyUrl());
    }

    public static void setCheckClearTextPermitted(boolean permitted) {
        AwContentsStaticsJni.get().setCheckClearTextPermitted(permitted);
    }

    public static void logCommandLineForDebugging() {
        AwContentsStaticsJni.get().logCommandLineForDebugging();
    }

    public static void logFlagOverridesWithNative(Map<String, Boolean> flagOverrides) {
        // Do work asynchronously to avoid blocking startup.
        PostTask.postTask(
                TaskTraits.BEST_EFFORT,
                () -> {
                    FlagOverrideHelper helper =
                            new FlagOverrideHelper(ProductionSupportedFlagList.sFlagList);
                    ArrayList<String> switches = new ArrayList<>();
                    ArrayList<String> features = new ArrayList<>();
                    for (Map.Entry<String, Boolean> entry : flagOverrides.entrySet()) {
                        Flag flag = helper.getFlagForName(entry.getKey());
                        boolean enabled = entry.getValue();
                        if (flag.isBaseFeature()) {
                            features.add(flag.getName() + (enabled ? ":enabled" : ":disabled"));
                        } else if (enabled) {
                            switches.add("--" + flag.getName());
                        }
                        // Only insert enabled switches; ignore explicitly disabled switches since
                        // this is usually a NOOP.
                    }
                    AwContentsStaticsJni.get()
                            .logFlagMetrics(
                                    switches.toArray(new String[0]),
                                    features.toArray(new String[0]));
                });
    }

    /**
     * Return the first substring consisting of the address of a physical location.
     * @see {@link android.webkit.WebView#findAddress(String)}
     *
     * @param addr The string to search for addresses.
     * @return the address, or if no address is found, return null.
     */
    public static String findAddress(String addr) {
        if (addr == null) {
            throw new NullPointerException("addr is null");
        }
        return FindAddress.findAddress(addr);
    }

    /** Returns true if WebView is running in multi process mode. */
    public static boolean isMultiProcessEnabled() {
        return AwContentsStaticsJni.get().isMultiProcessEnabled();
    }

    /** Returns the variations header used with the X-Client-Data header. */
    public static String getVariationsHeader() {
        String header = AwContentsStaticsJni.get().getVariationsHeader();
        RecordHistogram.recordCount100Histogram(
                "Android.WebView.VariationsHeaderLength", header.length());
        return header;
    }

    @NativeMethods
    interface Natives {
        void logCommandLineForDebugging();

        void logFlagMetrics(String[] switches, String[] features);

        String getSafeBrowsingPrivacyPolicyUrl();

        void clearClientCertPreferences(Runnable callback);

        String getUnreachableWebDataUrl();

        String getProductVersion();

        void setSafeBrowsingAllowlist(String[] urls, Callback<Boolean> callback);

        void setCheckClearTextPermitted(boolean permitted);

        boolean isMultiProcessEnabled();

        String getVariationsHeader();
    }
}
