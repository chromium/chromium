// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.provider.Settings;

import org.chromium.android_webview.devui.util.SafeIntentUtils;
import org.chromium.android_webview.nonembedded_util.WebViewPackageHelper;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;

import java.util.Locale;

/**
 * A helper class to yield an error if the the UI is launched from a different WebView package other
 * than the selected package by the system. It shows a persistent error message at the top of the
 * activity's root linear layout.
 */
public class WebViewPackageError {
    private static final String TAG = "WebViewDevTools";

    private PersistentErrorView mErrorMessage;
    private Activity mContext;

    public static final String OPEN_WEBVIEW_PROVIDER_BUTTON_TEXT =
            "Open DevTools in current provider";
    public static final String CHANGE_WEBVIEW_PROVIDER_BUTTON_TEXT = "Change provider";
    // The developer UI application label should be used in the placeholder.
    public static final String DIFFERENT_WEBVIEW_PROVIDER_ERROR_MESSAGE =
            "%s is not the system's currently selected WebView provider";
    // The developer UI application label should be used in the placeholder.
    public static final String DIFFERENT_WEBVIEW_PROVIDER_DIALOG_MESSAGE =
            "You are using DevTools for (%s) which is not the system's currently selected "
                    + "WebView provider";

    private static final String NO_VALID_WEBVIEW_MESSAGE =
            "Cannot find a valid WebView provider installed. "
                    + "Please install a valid WebView package. Contact "
                    + "android-webview-dev@chromium.org for help.";

    /**
     * @param context The {@link Activity} where the error is yield.
     * @param linearLayout the linearLayout to show error message at it's top.
     */
    public WebViewPackageError(Activity context, PersistentErrorView errorView) {
        mContext = context;
        mErrorMessage = errorView;
    }

    /**
     * Show the persistent error message at the top of the LinearLayout, if the system uses a
     * different WebView implementation. Hide it otherwise.
     */
    public boolean showMessageIfDifferent() {
        if (WebViewPackageHelper.isCurrentSystemWebViewImplementation(mContext)) {
            mErrorMessage.hide();
            return false;
        } else {
            buildErrorMessage();
            mErrorMessage.show();
            return true;
        }
    }

    private void buildErrorMessage() {
        if (!WebViewPackageHelper.hasValidWebViewImplementation(mContext)) {
            mErrorMessage.setText(NO_VALID_WEBVIEW_MESSAGE);
            // Unset action button and dialog. In case if the device got into the state where there
            // is no valid WebView implementation while the UI is running and the button and dialog
            // were set for other actions.
            mErrorMessage.setActionButton(null, null);
            mErrorMessage.setDialog(null);
            return;
        }

        AlertDialog.Builder dialogBuilder = new AlertDialog.Builder(mContext);
        CharSequence label = WebViewPackageHelper.loadLabel(mContext);
        mErrorMessage.setText(
                String.format(Locale.US, DIFFERENT_WEBVIEW_PROVIDER_ERROR_MESSAGE, label));
        dialogBuilder.setTitle("Different WebView Provider");
        dialogBuilder.setMessage(
                String.format(Locale.US, DIFFERENT_WEBVIEW_PROVIDER_DIALOG_MESSAGE, label));

        boolean canOpenCurrentProvider = canOpenCurrentWebViewProviderDevTools();
        boolean canChangeProvider = canAccessWebViewProviderDeveloperSetting();
        if (canChangeProvider) {
            mErrorMessage.setActionButton(
                    CHANGE_WEBVIEW_PROVIDER_BUTTON_TEXT, v -> openChangeWebViewProviderSettings());
        } else if (canOpenCurrentProvider) {
            mErrorMessage.setActionButton(
                    OPEN_WEBVIEW_PROVIDER_BUTTON_TEXT, v -> openCurrentWebViewProviderDevTools());
        }

        if (canOpenCurrentProvider) {
            dialogBuilder.setPositiveButton(
                    OPEN_WEBVIEW_PROVIDER_BUTTON_TEXT,
                    (d, id) -> openCurrentWebViewProviderDevTools());
        }
        if (canChangeProvider) {
            dialogBuilder.setNeutralButton(
                    CHANGE_WEBVIEW_PROVIDER_BUTTON_TEXT,
                    (d, id) -> openChangeWebViewProviderSettings());
        }

        mErrorMessage.setDialog(dialogBuilder.create());
    }

    private Dialog buildNoValidWebViewPackageDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(mContext);
        builder.setTitle("No Valid WebView").setMessage(NO_VALID_WEBVIEW_MESSAGE);
        return builder.create();
    }

    private Dialog buildNoDevToolsDialog() {
        PackageInfo systemWebViewPackage = WebViewPackageHelper.getCurrentWebViewPackage(mContext);
        AlertDialog.Builder builder = new AlertDialog.Builder(mContext);
        builder.setTitle("DevTools Not Found")
                .setMessage(
                        String.format(
                                Locale.US,
                                "DevTools are not available in the current "
                                        + "WebView provider selected by the system (%s).\n\n"
                                        + "Please update to a newer version or select a different WebView "
                                        + "provider.",
                                systemWebViewPackage.packageName));

        if (canAccessWebViewProviderDeveloperSetting()) {
            builder.setPositiveButton(
                    CHANGE_WEBVIEW_PROVIDER_BUTTON_TEXT,
                    (dialog, id) -> openChangeWebViewProviderSettings());
        }

        return builder.create();
    }

    /** Check if the system current selceted WebView provider has a valid DevTools implementation. */
    private boolean canOpenCurrentWebViewProviderDevTools() {
        PackageInfo systemWebViewPackage = WebViewPackageHelper.getCurrentWebViewPackage(mContext);
        if (systemWebViewPackage == null) {
            Log.e(TAG, "Could not find a valid WebView implementation");
            return false;
        }
        return PackageManagerUtils.canResolveActivity(
                buildWebViewDevUiIntent(systemWebViewPackage.packageName));
    }

    private void openCurrentWebViewProviderDevTools() {
        if (!WebViewPackageHelper.hasValidWebViewImplementation(mContext)) {
            buildNoValidWebViewPackageDialog().show();
            return;
        }
        if (!canOpenCurrentWebViewProviderDevTools()) {
            buildNoDevToolsDialog().show();
            return;
        }
        PackageInfo systemWebViewPackage = WebViewPackageHelper.getCurrentWebViewPackage(mContext);
        Intent intent = buildWebViewDevUiIntent(systemWebViewPackage.packageName);
        mContext.startActivity(intent);
    }

    /** Builds the intent used to open DevTools from the given {@code packageName}. */
    private Intent buildWebViewDevUiIntent(String packageName) {
        Intent intent = new Intent("com.android.webview.SHOW_DEV_UI");
        intent.setPackage(packageName);
        // If the UI is already launched then clear its task stack.
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK);
        // Launch the new UI in a new task.
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    /** Check if the user can open the settings activity to change WebView providers or not. */
    public static boolean canAccessWebViewProviderDeveloperSetting() {
        // Switching WebView providers is possible from API >= 24.
        // The activity to change WebView provider is only enabled for admin user, see
        // https://crbug.com/1347418#comment8.
        return PackageManagerUtils.canResolveActivity(new Intent(Settings.ACTION_WEBVIEW_SETTINGS));
    }

    private void openChangeWebViewProviderSettings() {
        SafeIntentUtils.startActivityOrShowError(
                mContext,
                new Intent(Settings.ACTION_WEBVIEW_SETTINGS),
                SafeIntentUtils.WEBVIEW_SETTINGS_ERROR);
    }
}
