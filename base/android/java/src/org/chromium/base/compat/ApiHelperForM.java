// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.compat;

import android.annotation.TargetApi;
import android.app.Activity;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkInfo;
import android.os.Build;
import android.os.Process;
import android.security.NetworkSecurityPolicy;
import android.view.ActionMode;
import android.view.ViewConfiguration;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import org.chromium.base.annotations.VerifiesOnM;

/**
 * Utility class to use new APIs that were added in M (API level 23). These need to exist in a
 * separate class so that Android framework can successfully verify classes without
 * encountering the new APIs.
 */
@VerifiesOnM
@TargetApi(Build.VERSION_CODES.M)
public final class ApiHelperForM {
    private ApiHelperForM() {}

    /**
     * See {@link WebViewClient#onPageCommitVisible(WebView, String)}, which was added in M.
     */
    public static void onPageCommitVisible(
            WebViewClient webViewClient, WebView webView, String url) {
        webViewClient.onPageCommitVisible(webView, url);
    }

    /**
     * See {@link Process#is64Bit()}.
     */
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
        return activity.getPackageManager().isPermissionRevokedByPolicy(
                permission, activity.getPackageName());
    }

    /** See {@link NetworkSecurityPolicy#isCleartextTrafficPermitted()}. */
    public static boolean isCleartextTrafficPermitted() {
        return NetworkSecurityPolicy.getInstance().isCleartextTrafficPermitted();
    }

    /*
     * See {@link ActionMode#invalidateContentRect()}.
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
}
