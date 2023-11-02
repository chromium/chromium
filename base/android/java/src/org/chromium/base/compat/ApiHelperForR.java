// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.compat;

import android.app.ActivityManager;
import android.content.Context;
import android.graphics.Rect;
import android.hardware.input.InputManager;
import android.net.Uri;
import android.os.Build;
import android.os.storage.StorageManager;
import android.view.Display;
import android.view.InputEvent;
import android.view.VerifiedInputEvent;
import android.view.WindowManager;

import androidx.annotation.RequiresApi;

import java.io.File;

/**
 * Utility class to use new APIs that were added in R (API level 30). These need to exist in a
 * separate class so that Android framework can successfully verify classes without
 * encountering the new APIs.
 */
@RequiresApi(Build.VERSION_CODES.R)
public final class ApiHelperForR {
    private ApiHelperForR() {}

    public static Display getDisplay(Context context) throws UnsupportedOperationException {
        return context.getDisplay();
    }

    /**
     * See {@link StorageManager#getStorageVolume(Uri)}.
     * See {@link File#getDirectory()}.
     */
    public static File getVolumeDir(StorageManager manager, Uri uri) {
        return manager.getStorageVolume(uri).getDirectory();
    }

    /**
     * See {@link InputManager#verifyInputEvent(InputEvent)}.
     */
    public static VerifiedInputEvent verifyInputEvent(InputManager manager, InputEvent inputEvent) {
        return manager.verifyInputEvent(inputEvent);
    }

    /**
     * See {@link android.app.ActivityManager#setProcessStateSummary(byte[])}
     */
    public static void setProcessStateSummary(ActivityManager am, byte[] bytes) {
        am.setProcessStateSummary(bytes);
    }

    /**
     * See {@link WindowManager#getMaximumWindowMetrics()}.
     * See {@link WindowMetrics#getBounds()}.
     */
    public static Rect getMaximumWindowMetricsBounds(WindowManager manager) {
        return manager.getMaximumWindowMetrics().getBounds();
    }
}
