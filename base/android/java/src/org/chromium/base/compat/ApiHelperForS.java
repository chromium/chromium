// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.compat;

import android.Manifest;
import android.app.ForegroundServiceStartNotAllowedException;
import android.app.Notification;
import android.app.PendingIntent;
import android.app.PictureInPictureParams;
import android.app.Service;
import android.content.ClipData;
import android.content.ClipDescription;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Process;
import android.view.Display;
import android.view.textclassifier.TextClassification;
import android.view.textclassifier.TextLinks;
import android.view.textclassifier.TextSelection;

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;

/**
 * Utility class to use new APIs that were added in S (API level 31). These need to exist in a
 * separate class so that Android framework can successfully verify classes without
 * encountering the new APIs.
 */
@RequiresApi(Build.VERSION_CODES.S)
public final class ApiHelperForS {
    private static final String TAG = "ApiHelperForS";

    private ApiHelperForS() {}

    /** See {@link ClipDescription#isStyleText()}. */
    public static boolean isStyleText(ClipDescription clipDescription) {
        return clipDescription.isStyledText();
    }

    /** See {@link ClipDescription#getConfidenceScore()}. */
    public static float getConfidenceScore(
            ClipDescription clipDescription, @NonNull String entityType) {
        return clipDescription.getConfidenceScore(entityType);
    }

    /**
     * Return true if {@link ClipDescription#getClassificationStatus()} returns
     * ClipDescription.CLASSIFICATION_COMPLETE.
     */
    public static boolean isGetClassificationStatusIsComplete(ClipDescription clipDescription) {
        return clipDescription.getClassificationStatus() == ClipDescription.CLASSIFICATION_COMPLETE;
    }

    /** See {@link ClipData.Item#getTextLinks()}. */
    public static TextLinks getTextLinks(ClipData.Item item) {
        return item.getTextLinks();
    }

    public static boolean hasBluetoothConnectPermission() {
        return ApiCompatibilityUtils.checkPermission(
                        ContextUtils.getApplicationContext(),
                        Manifest.permission.BLUETOOTH_CONNECT,
                        Process.myPid(),
                        Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }

    /** See {@link android.app.PictureInPictureParams.Builder#setAutoEnterEnabled(boolean)} */
    public static void setAutoEnterEnabled(
            PictureInPictureParams.Builder builder, boolean enabled) {
        builder.setAutoEnterEnabled(enabled);
    }

    /**
     * See {@link android.view.textclassifier.TextSelection.Request.
     * Builder#setIncludeTextClassification(boolean)}
     */
    public static TextSelection.Request.Builder setIncludeTextClassification(
            TextSelection.Request.Builder builder, boolean includeTextClassification) {
        return builder.setIncludeTextClassification(includeTextClassification);
    }

    /** See {@link android.view.textclassifier.TextSelection#getTextClassification()} */
    public static TextClassification getTextClassification(TextSelection textSelection) {
        return textSelection.getTextClassification();
    }

    /** See Context#createWindowContext. */
    public static Context createWindowContext(
            Context context, Display display, int type, Bundle options) {
        return context.createWindowContext(display, type, options);
    }

    /** See {@link PendingIntent#FLAG_MUTABLE}. */
    public static int getPendingIntentMutableFlag() {
        return PendingIntent.FLAG_MUTABLE;
    }

    /** See {@link Service#startForegroung(int, Notification, int) }. */
    public static void startForeground(
            Service service, int id, Notification notification, int foregroundServiceType) {
        try {
            service.startForeground(id, notification, foregroundServiceType);
        } catch (ForegroundServiceStartNotAllowedException e) {
            Log.e(
                    TAG,
                    "Cannot run service as foreground: "
                            + e
                            + " for notification channel "
                            + notification.getChannelId()
                            + " notification id "
                            + id);
        }
    }
}
