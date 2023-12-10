// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.compat;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Icon;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkInfo;
import android.os.Build;
import android.os.PowerManager;
import android.os.Process;
import android.os.UserManager;
import android.security.NetworkSecurityPolicy;
import android.view.ActionMode;
import android.view.Display;
import android.view.MotionEvent;
import android.view.ViewConfiguration;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import androidx.annotation.RequiresApi;

/**
 * Utility class to use new APIs that were added in M (API level 23). These need to exist in a
 * separate class so that Android framework can successfully verify classes without
 * encountering the new APIs.
 */
@RequiresApi(Build.VERSION_CODES.M)
public final class ApiHelperForM {
    private ApiHelperForM() {}

    /** See {@link WebViewClient#onPageCommitVisible(WebView, String)}, which was added in M. */
    public static void onPageCommitVisible(
            WebViewClient webViewClient, WebView webView, String url) {
        webViewClient.onPageCommitVisible(webView, url);
    }

    /** See {@link Process#is64Bit()}. */
    public static boolean isProcess64Bit() {
        return Process.is64Bit();
    }

    /** See {@link ConnectivityManager#getBoundNetworkForProcess() } */
    public static Network getBoundNetworkForProcess(ConnectivityManager connectivityManager) {
        return connectivityManager.getBoundNetworkForProcess();
    }

    /** See {@link Network#getNetworkHandle() } */
    public static long getNetworkHandle(Network network) {
        return network.getNetworkHandle();
    }

    /** See @{link ConnectivityManager#getActiveNetwork() } */
    public static Network getActiveNetwork(ConnectivityManager connectivityManager) {
        return connectivityManager.getActiveNetwork();
    }

    /** See @{link ConnectivityManager#getNetworkInfo(Network) } */
    public static NetworkInfo getNetworkInfo(
            ConnectivityManager connectivityManager, Network network) {
        return connectivityManager.getNetworkInfo(network);
    }

    /** See {@link Activity#requestPermissions(String[], int)}. */
    public static void requestActivityPermissions(
            Activity activity, String[] permissions, int requestCode) {
        activity.requestPermissions(permissions, requestCode);
    }

    /** See {@link Activity#shouldShowRequestPermissionRationale(String)}. */
    public static boolean shouldShowRequestPermissionRationale(
            Activity activity, String permission) {
        return activity.shouldShowRequestPermissionRationale(permission);
    }

    /** See {@link PackageManager#isPermissionRevokedByPolicy(String, String)}. */
    public static boolean isPermissionRevokedByPolicy(Activity activity, String permission) {
        return activity.getPackageManager()
                .isPermissionRevokedByPolicy(permission, activity.getPackageName());
    }

    /** See {@link NetworkSecurityPolicy#isCleartextTrafficPermitted()}. */
    public static boolean isCleartextTrafficPermitted() {
        return NetworkSecurityPolicy.getInstance().isCleartextTrafficPermitted();
    }

    /** See {@link UserManager#isSystemUser()}. */
    public static boolean isSystemUser(UserManager userManager) {
        return userManager.isSystemUser();
    }

    /**
     * See {@link ActionMode#invalidateContentRect()}.
     *
     * @param actionMode
     */
    public static void invalidateContentRectOnActionMode(ActionMode actionMode) {
        actionMode.invalidateContentRect();
    }

    public static void onWindowFocusChangedOnActionMode(ActionMode actionMode, boolean gainFocus) {
        actionMode.onWindowFocusChanged(gainFocus);
    }

    public static int getActionModeType(ActionMode actionMode) {
        return actionMode.getType();
    }

    public static long getDefaultActionModeHideDuration() {
        return ViewConfiguration.getDefaultActionModeHideDuration();
    }

    public static void hideActionMode(ActionMode actionMode, long duration) {
        actionMode.hide(duration);
    }

    public static int getPendingIntentImmutableFlag() {
        return PendingIntent.FLAG_IMMUTABLE;
    }

    /** See {@link ConnectivityManager#reportNetworkConnectivity(Network, boolean)}. */
    public static void reportNetworkConnectivity(
            ConnectivityManager connectivityManager, Network network, boolean hasConnectivity) {
        connectivityManager.reportNetworkConnectivity(network, hasConnectivity);
    }

    /** See {@link MotionEvent#getActionButton() }. */
    public static int getActionButton(MotionEvent event) {
        return event.getActionButton();
    }

    /** See {@link Display.Mode#getPhysicalWidth() }. */
    public static int getModePhysicalWidth(Display.Mode mode) {
        return mode.getPhysicalWidth();
    }

    /** See {@link Display.Mode#getPhysicalHeight() }. */
    public static int getModePhysicalHeight(Display.Mode mode) {
        return mode.getPhysicalHeight();
    }

    /** See {@link Icon#createWithBitmap(Bitmap) }. */
    public static Icon createIconWithBitmap(Bitmap bitmap) {
        return Icon.createWithBitmap(bitmap);
    }

    /** See {@link PowerManager#isDeviceIdleMode() }. */
    public static boolean isDeviceIdleMode(PowerManager powerManager) {
        return powerManager.isDeviceIdleMode();
    }

    /** See {@link Context#getSystemService(Class<T>)}. */
    public static <T> T getSystemService(Context context, Class<T> serviceClass) {
        return context.getSystemService(serviceClass);
    }
}
