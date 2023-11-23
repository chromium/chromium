// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Looper;

import androidx.annotation.IntDef;

import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwDevToolsServer;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.MemoryPressureLevel;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.memory.MemoryPressureMonitor;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.util.List;

/**
 * This class provides functionality that is accessed in a static way from apps using WebView.
 * This class is meant to be shared between the webkit-glue layer and the support library glue
 * layer.
 * Ideally this class would live in a lower layer than the webkit-glue layer, to allow sharing the
 * implementation between different glue layers without needing to depend on the webkit-glue layer
 * (right now there are dependencies from this class on the webkit-glue layer though).
 */
@Lifetime.Singleton
public class SharedStatics {
    private AwDevToolsServer mDevToolsServer;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        ApiCall.FIND_ADDRESS, ApiCall.GET_DEFAULT_USER_AGENT,
        ApiCall.SET_WEB_CONTENTS_DEBUGGING_ENABLED, ApiCall.CLEAR_CLIENT_CERT_PREFERENCES,
        ApiCall.ENABLE_SLOW_WHOLE_DOCUMENT_DRAW, ApiCall.GET_SAFE_BROWSING_PRIVACY_POLICY_URL,
        // Add new constants above. The final constant should have a trailing comma for
        // cleaner diffs.
        ApiCall.COUNT, // Added to suppress WrongConstant in #recordStaticApiCall
    })
    public @interface ApiCall {
        int FIND_ADDRESS = 0;
        int GET_DEFAULT_USER_AGENT = 1;
        int SET_WEB_CONTENTS_DEBUGGING_ENABLED = 2;
        int CLEAR_CLIENT_CERT_PREFERENCES = 3;
        int ENABLE_SLOW_WHOLE_DOCUMENT_DRAW = 4;
        int GET_SAFE_BROWSING_PRIVACY_POLICY_URL = 5;
        // Remember to update StaticWebViewApiCall in enums.xml when adding new values here
        int COUNT = 6;
    }

    public static void recordStaticApiCall(@ApiCall int sample) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.ApiCall.Static", sample, ApiCall.COUNT);
    }

    public SharedStatics() {}

    public String findAddress(String addr) {
        try (TraceEvent event = TraceEvent.scoped("WebView.APICall.Framework.FIND_ADDRESS")) {
            recordStaticApiCall(ApiCall.FIND_ADDRESS);
            return AwContentsStatics.findAddress(addr);
        }
    }

    public String getDefaultUserAgent(Context context) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.GET_DEFAULT_USER_AGENT")) {
            recordStaticApiCall(ApiCall.GET_DEFAULT_USER_AGENT);
            return AwSettings.getDefaultUserAgent();
        }
    }

    public void setWebContentsDebuggingEnabled(boolean enable) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.SET_WEB_CONTENTS_DEBUGGING_ENABLED")) {
            recordStaticApiCall(ApiCall.SET_WEB_CONTENTS_DEBUGGING_ENABLED);
            // On debug builds, Web Contents debugging is enabled elsewhere, and cannot be disabled.
            if (BuildInfo.isDebugAndroidOrApp()) return;
            setWebContentsDebuggingEnabledUnconditionally(enable);
        }
    }

    public void setWebContentsDebuggingEnabledUnconditionally(boolean enable) {
        if (Looper.myLooper() != ThreadUtils.getUiThreadLooper()) {
            throw new RuntimeException(
                    "Toggling of Web Contents Debugging must be done on the UI thread");
        }
        if (mDevToolsServer == null) {
            if (!enable) return;
            mDevToolsServer = new AwDevToolsServer();
        }
        mDevToolsServer.setRemoteDebuggingEnabled(enable);
    }

    public void clearClientCertPreferences(Runnable onCleared) {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.CLEAR_CLIENT_CERT_PREFERENCES")) {
            recordStaticApiCall(ApiCall.CLEAR_CLIENT_CERT_PREFERENCES);
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT,
                    () -> AwContentsStatics.clearClientCertPreferences(onCleared));
        }
    }

    public void freeMemoryForTests() {
        if (ActivityManager.isRunningInTestHarness()) {
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        // This variable is needed to prevent weird formatting by "git cl format".
                        MemoryPressureMonitor pressureMonitor = MemoryPressureMonitor.INSTANCE;
                        pressureMonitor.notifyPressure(MemoryPressureLevel.CRITICAL);
                    });
        }
    }

    public void enableSlowWholeDocumentDraw() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.ENABLE_SLOW_WHOLE_DOCUMENT_DRAW")) {
            recordStaticApiCall(ApiCall.ENABLE_SLOW_WHOLE_DOCUMENT_DRAW);
            WebViewChromium.enableSlowWholeDocumentDraw();
        }
    }

    public Uri[] parseFileChooserResult(int resultCode, Intent intent) {
        return AwContentsClient.parseFileChooserResult(resultCode, intent);
    }

    /**
     * Starts Safe Browsing initialization. This should only be called once.
     * @param context is the application context the WebView will be used in.
     * @param callback will be called with the value true if initialization is
     * successful. The callback will be run on the UI thread.
     */
    public void initSafeBrowsing(Context context, Callback<Boolean> callback) {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT, () -> AwContentsStatics.initSafeBrowsing(context, callback));
    }

    public void setSafeBrowsingAllowlist(List<String> urls, Callback<Boolean> callback) {
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> AwContentsStatics.setSafeBrowsingAllowlist(urls, callback));
    }

    /**
     * Returns a URL pointing to the privacy policy for Safe Browsing reporting.
     *
     * @return the url pointing to a privacy policy document which can be displayed
     * to users.
     */
    public Uri getSafeBrowsingPrivacyPolicyUrl() {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.Framework.GET_SAFE_BROWSING_PRIVACY_POLICY_URL")) {
            recordStaticApiCall(ApiCall.GET_SAFE_BROWSING_PRIVACY_POLICY_URL);
            return PostTask.runSynchronously(
                    TaskTraits.UI_DEFAULT,
                    () -> AwContentsStatics.getSafeBrowsingPrivacyPolicyUrl());
        }
    }

    public boolean isMultiProcessEnabled() {
        return AwContentsStatics.isMultiProcessEnabled();
    }

    public String getVariationsHeader() {
        return AwContentsStatics.getVariationsHeader();
    }
}
