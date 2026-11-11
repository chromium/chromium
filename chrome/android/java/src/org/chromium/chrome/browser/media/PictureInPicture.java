// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.app.AppOpsManager;
import android.app.AppOpsManager.OnOpChangedListener;
import android.content.Context;
import android.content.pm.PackageManager;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.util.BrowserUiUtils;

/**
 * Utility for determining if Picture-in-Picture is available and whether the user has disabled
 * Picture-in-Picture for Chrome using the system's per-application settings.
 */
@NullMarked
public abstract class PictureInPicture {
    private static @Nullable Boolean sIsPictureInPictureEnabled;
    private static @Nullable OnOpChangedListener sOnOpChangedListener;

    private PictureInPicture() {}

    /**
     * Determines whether Picture-in-Picture is enabled for the activity represented by |context|.
     * Picture-in-Picture may be disabled because the device or display does not support it, or
     * because the user or a management tool has explicitly disallowed the Chrome App to enter
     * Picture-in-Picture.
     *
     * <p>This intentionally does not check the Android version, so that it can be shared by all
     * Picture-in-Picture entry points, including fullscreen video Picture-in-Picture on Android Q.
     * Callers that require a minimum Android version must check it themselves. For example, web
     * Picture-in-Picture requires Android R; see
     * ActivityTabWebContentsDelegateAndroid#isPictureInPictureEnabled.
     *
     * @param context The Activity context to check whether it can enter Picture-in-Picture.
     * @return boolean true if Picture-In-Picture is enabled, otherwise false.
     */
    public static boolean isEnabled(Context context) {
        if (BrowserUiUtils.isAndroidAutoProjected(context)) {
            return false;
        }
        // Some devices may not support PiP, such as automotive. See b/267249289.
        if (!context.getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_PICTURE_IN_PICTURE)) {
            return false;
        }

        ThreadUtils.assertOnUiThread();
        if (sIsPictureInPictureEnabled != null) {
            return sIsPictureInPictureEnabled;
        }

        try (TraceEvent _ = TraceEvent.scoped("PictureInPicture::isEnabled")) {
            Context appContext = context.getApplicationContext();
            final AppOpsManager appOpsManager =
                    (AppOpsManager) appContext.getSystemService(Context.APP_OPS_SERVICE);
            final int status =
                    appOpsManager.checkOpNoThrow(
                            AppOpsManager.OPSTR_PICTURE_IN_PICTURE,
                            appContext.getApplicationInfo().uid,
                            appContext.getPackageName());

            boolean isEnabled = (status == AppOpsManager.MODE_ALLOWED);
            sIsPictureInPictureEnabled = isEnabled;
            if (sOnOpChangedListener == null) {
                sOnOpChangedListener =
                        (op, packageName) ->
                                ThreadUtils.runOnUiThread(() -> sIsPictureInPictureEnabled = null);
                appOpsManager.startWatchingMode(
                        AppOpsManager.OPSTR_PICTURE_IN_PICTURE,
                        appContext.getPackageName(),
                        sOnOpChangedListener);
                ResettersForTesting.register(
                        () -> {
                            sIsPictureInPictureEnabled = null;
                            if (sOnOpChangedListener != null) {
                                appOpsManager.stopWatchingMode(sOnOpChangedListener);
                                sOnOpChangedListener = null;
                            }
                        });
            }

            return isEnabled;
        }
    }
}
