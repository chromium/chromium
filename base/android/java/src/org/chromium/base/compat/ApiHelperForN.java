// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.compat;

import android.app.Activity;
import android.app.Notification;
import android.app.job.JobInfo;
import android.app.job.JobScheduler;
import android.content.ClipData;
import android.content.ComponentName;
import android.content.pm.PackageManager;
import android.media.MediaCodec.CryptoInfo;
import android.net.ConnectivityManager;
import android.os.Build;
import android.os.Process;
import android.security.NetworkSecurityPolicy;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;
import android.view.View.DragShadowBuilder;
import android.webkit.WebResourceRequest;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.RemoteViews;

import androidx.annotation.RequiresApi;

/**
 * Utility class to use new APIs that were added in N (API level 24). These need to exist in a
 * separate class so that Android framework can successfully verify classes without
 * encountering the new APIs.
 */
@RequiresApi(Build.VERSION_CODES.N)
public final class ApiHelperForN {
    private ApiHelperForN() {}

    /**
     * See {@link WebViewClient#shouldOverrideUrlLoading(WebView, WebResourceRequest)}, which was
     * added in N.
     */
    public static boolean shouldOverrideUrlLoading(
            WebViewClient webViewClient, WebView webView, WebResourceRequest request) {
        return webViewClient.shouldOverrideUrlLoading(webView, request);
    }

    /** See {@link JobScheduler#getPendingJob(int)}. */
    public static JobInfo getPendingJob(JobScheduler scheduler, int jobId) {
        return scheduler.getPendingJob(jobId);
    }

    /** See {@link View#startDragAndDrop(ClipData, DragShadowBuilder, Object, int)}. */
    public static boolean startDragAndDrop(View view, ClipData data,
            DragShadowBuilder shadowBuilder, Object myLocalState, int flags) {
        return view.startDragAndDrop(data, shadowBuilder, myLocalState, flags);
    }

    /** See {@link CryptoInfo#setPattern(Pattern)}. */
    public static void setCryptoInfoPattern(CryptoInfo cryptoInfo, int encrypt, int skip) {
        cryptoInfo.setPattern(new CryptoInfo.Pattern(encrypt, skip));
    }

    /** See {@link Activity#setVrModeEnabled(boolean, ComponentName)}. */
    public static void setVrModeEnabled(Activity activity, boolean enabled,
            ComponentName requestedComponent) throws PackageManager.NameNotFoundException {
        activity.setVrModeEnabled(enabled, requestedComponent);
    }

    /** See {@link NetworkSecurityPolicy#isCleartextTrafficPermitted(String)}. */
    public static boolean isCleartextTrafficPermitted(String host) {
        return NetworkSecurityPolicy.getInstance().isCleartextTrafficPermitted(host);
    }

    /** See {@link View#onResolvePointerIcon(MotionEvent, int)}. */
    public static PointerIcon onResolvePointerIcon(View view, MotionEvent event, int pointerIndex) {
        return view.onResolvePointerIcon(event, pointerIndex);
    }

    /** See {@link Process#getStartUptimeMillis()}. */
    public static long getStartUptimeMillis() {
        return Process.getStartUptimeMillis();
    }

    /** See {@link Notification.Builder#setCustomContentView(RemoteViews)}. */
    public static Notification.Builder setCustomContentView(
            Notification.Builder builder, RemoteViews views) {
        return builder.setCustomContentView(views);
    }

    /** See {@link Notification.Builder#setCustomBigContentView(RemoteViews)}. */
    public static Notification.Builder setCustomBigContentView(
            Notification.Builder builder, RemoteViews view) {
        return builder.setCustomBigContentView(view);
    }

    /** See {@link ConnectivityManager#getRestrictBackgroundStatus(ConnectivityManager)}. */
    public static int getRestrictBackgroundStatus(ConnectivityManager cm) {
        return cm.getRestrictBackgroundStatus();
    }
}
