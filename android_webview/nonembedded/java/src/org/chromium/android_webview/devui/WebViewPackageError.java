// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.ResolveInfo;
import android.provider.Settings;
import android.widget.LinearLayout;

import org.chromium.android_webview.devui.util.WebViewPackageHelper;
import org.chromium.base.Log;

import java.util.List;
import java.util.Locale;

/**
 * A helper class to yield an error if the the UI is launched from a different WebView package other
 * than the selected package by the system. It shows a persistent error message at the top of the
 * activity's root linear layout as well as an alert dialogue to change WebView implementation.
 */
public class WebViewPackageError {
    private static final String TAG = "WebViewDevTools";

    private PersistentErrorView mErrorMessage;
    private Activity mContext;
    private Dialog mErrorDialog;

    /**
     * @param context The {@link Activity} where the error is yield.
     * @param linearLayout the linearLayout to show error message at it's top.
     */
    public WebViewPackageError(Activity context, LinearLayout linearLayout) {
        mContext = context;

        mErrorDialog = buildDifferentPackageErrorDialog(mContext.getPackageName());
        mErrorMessage = new PersistentErrorView(context, PersistentErrorView.Type.WARNING)
                                .prependToLinearLayout(linearLayout)
                                .setText("Warning: different WebView provider - Tap for more info.")
                                .setDialog(mErrorDialog);
    }

    /**
     * Show the persistent error message at the top of the LinearLayout, if the system uses a
     * different WebView implementation. Hide it otherwise.
     */
    public void showMessageIfDifferent() {
        if (isWebViewPackageDifferent()) {
            mErrorMessage.show();
        } else {
            mErrorMessage.hide();
        }
    }

    /**
     * Show an {@link AlertDialog} about the different WebView package error with an action to
     * launch system settings to show and change WebView implementation.
     */
    public void showDialogIfDifferent() {
        if (isWebViewPackageDifferent()) {
            mErrorDialog.show();
        }
    }

    private boolean isWebViewPackageDifferent() {
        PackageInfo systemWebViewPackage = WebViewPackageHelper.getCurrentWebViewPackage(mContext);
        if (systemWebViewPackage == null) {
            Log.e(TAG, "Could not find a valid WebView implementation");
            buildNoValidWebViewPackageDialog().show();
            return true;
        }
        return !mContext.getPackageName().equals(systemWebViewPackage.packageName);
    }

    private Dialog buildDifferentPackageErrorDialog(String currentWebViewPackage) {
        AlertDialog.Builder builder = new AlertDialog.Builder(mContext);
        builder.setTitle("Wrong WebView DevTools")
                .setMessage(String.format(Locale.US,
                        "This app (%s) is not the selected system's WebView provider.",
                        currentWebViewPackage));
        builder.setPositiveButton("Open the current WebView provider", (dialog, id) -> {
            PackageInfo systemWebViewPackage =
                    WebViewPackageHelper.getCurrentWebViewPackage(mContext);
            if (systemWebViewPackage == null) {
                Log.e(TAG, "Could not find a valid WebView implementation");
                buildNoValidWebViewPackageDialog().show();
                return;
            }
            Intent intent = new Intent("com.android.webview.SHOW_DEV_UI");
            intent.setPackage(systemWebViewPackage.packageName);
            // Check if the intent is resolved, i.e current system WebView package has a developer
            // UI that responds to "com.android.webview.SHOW_DEV_UI" action.
            List<ResolveInfo> resolveInfo =
                    mContext.getPackageManager().queryIntentActivities(intent, 0);
            if (resolveInfo.isEmpty()) {
                buildNoDevToolsDialog(systemWebViewPackage.packageName).show();
                return;
            }
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            mContext.startActivity(intent);
            mContext.finishAndRemoveTask();
        });

        builder.setNeutralButton("Change WebView provider",
                (dialog, id)
                        -> mContext.startActivity(new Intent(Settings.ACTION_WEBVIEW_SETTINGS)));

        return builder.create();
    }

    private Dialog buildNoValidWebViewPackageDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(mContext);
        builder.setTitle("No Valid WebView")
                .setMessage("Cannot find a valid WebView provider installed. "
                        + "Please install a valid WebView package. Contact "
                        + "android-webview-dev@chromium.org for help.");

        return builder.create();
    }

    private Dialog buildNoDevToolsDialog(String sytemWebViewPackageName) {
        AlertDialog.Builder builder = new AlertDialog.Builder(mContext);
        builder.setTitle("DevTools Not Found")
                .setMessage(String.format(Locale.US,
                        "DevTools are not available in the current "
                                + "WebView provider selected by the system (%s).\n\n"
                                + "Please update to a newer version or select a different WebView "
                                + "provider.",
                        sytemWebViewPackageName));
        builder.setPositiveButton("Change WebView provider",
                (dialog, id)
                        -> mContext.startActivity(new Intent(Settings.ACTION_WEBVIEW_SETTINGS)));

        return builder.create();
    }
}
