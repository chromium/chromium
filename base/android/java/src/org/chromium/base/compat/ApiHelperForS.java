// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.compat;

import android.annotation.TargetApi;
import android.app.PictureInPictureParams;
import android.content.ClipData;
import android.content.ClipDescription;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Process;
import android.view.textclassifier.TextLinks;

import androidx.annotation.NonNull;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.VerifiesOnS;

/**
 * Utility class to use new APIs that were added in S (API level 31). These need to exist in a
 * separate class so that Android framework can successfully verify classes without
 * encountering the new APIs.
 */
@VerifiesOnS
@TargetApi(Build.VERSION_CODES.S)
public final class ApiHelperForS {
    private static final String TAG = "ApiHelperForS";

    private ApiHelperForS() {}

    /**
     * See {@link ClipDescription#isStyleText()}.
     */
    public static boolean isStyleText(ClipDescription clipDescription) {
        return clipDescription.isStyledText();
    }

    /**
     * See {@link ClipDescription#getConfidenceScore()}.
     */
    public static float getConfidenceScore(
            ClipDescription clipDescription, @NonNull String entityType) {
        return clipDescription.getConfidenceScore(entityType);
    }

    /**
     * See {@link ClipData.Item#getTextLinks()}.
     */
    public static TextLinks getTextLinks(ClipData.Item item) {
        return item.getTextLinks();
    }

    public static boolean hasBluetoothConnectPermission() {
        // TODO(b/183501112): Replace the permission string with the actual Manfifest constant once
        // Chrome starts compiling against the S SDK.
        return ApiCompatibilityUtils.checkPermission(ContextUtils.getApplicationContext(),
                       "android.permission.BLUETOOTH_CONNECT", Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * See {@link android.app.PictureInPictureParams.Builder#setAutoEnterEnabled(boolean)}
     */
    public static void setAutoEnterEnabled(
            PictureInPictureParams.Builder builder, boolean enabled) {
        builder.setAutoEnterEnabled(enabled);
    }
}
