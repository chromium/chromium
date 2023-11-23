// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui.util;

import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.IntentUtils;

/** Util methods to handle external intents. */
public class SafeIntentUtils {
    public static final String NO_BROWSER_FOUND_ERROR = "Can't find a browser to open URL";
    public static final String WEBVIEW_SETTINGS_ERROR =
            "Can't open WebView Settings for the current user";

    /**
     * Attempt starting an Activity using the given intent, otherwise show a dialog with the given
     * error message.
     */
    public static void startActivityOrShowError(
            Context context, Intent intent, String errorMessage) {
        if (!IntentUtils.safeStartActivity(context, intent)) {
            AlertDialog.Builder builder = new AlertDialog.Builder(context);
            builder.setMessage(errorMessage);
            builder.setNeutralButton("OK", (dialogInterface, i) -> {});
            builder.create().show();
        }
    }

    // Don't instantiate this class.
    private SafeIntentUtils() {}
}
