// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.app.AppOpsManager;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;

import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.util.BrowserUiUtils;

/**
 * Utility for determining if Picture-in-Picture is available and whether the user has disabled
 * Picture-in-Picture for Chrome using the system's per-application settings.
 */
@NullMarked
public abstract class PictureInPicture {
    private PictureInPicture() {}

    /**
     * Determines whether Picture-in-Picture is enabled for the activity represented by |context|.
     * Picture-in-Picture may be disabled because the device or display does not support it, or
     * because the user or a management tool has explicitly disallowed the Chrome App to enter
     * Picture-in-Picture.
     *
     * @param context The Activity context to check whether it can enter Picture-in-Picture.
     * @return boolean true if Picture-In-Picture is enabled, otherwise false.
     */
    public static boolean isEnabled(Context context) {
        // Some versions of Android crash when the activity enters Picture-in-Picture
        // immediately after it exits Picture-in-Picture. See b/143784148
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return false;
        }
        if (BrowserUiUtils.isAndroidAutoProjected(context)) {
            return false;
        }
        // Some devices may not support PiP, such as automotive. See b/267249289.
        if (!context.getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)) {
            return false;
        }
        try (TraceEvent _ = TraceEvent.scoped("PictureInPicture::isEnabled")) {
            final AppOpsManager appOpsManager =
                    (AppOpsManager) context.getSystemService(Context.APP_OPS_SERVICE);
            final int status =
                    appOpsManager.checkOpNoThrow(
                            AppOpsManager.OPSTR_PICTURE_IN_PICTURE,
                            context.getApplicationInfo().uid,
                            context.getPackageName());

            return (status == AppOpsManager.MODE_ALLOWED);
        }
    }
}
