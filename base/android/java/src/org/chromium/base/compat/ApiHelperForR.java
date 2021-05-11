// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.compat;

import android.annotation.TargetApi;
import android.content.Context;
import android.hardware.input.InputManager;
import android.net.Uri;
import android.os.Build;
import android.os.storage.StorageManager;
import android.view.Display;
import android.view.InputEvent;
import android.view.VerifiedInputEvent;

import org.chromium.base.annotations.VerifiesOnR;

import java.io.File;

/**
 * Utility class to use new APIs that were added in R (API level 30). These need to exist in a
 * separate class so that Android framework can successfully verify classes without
 * encountering the new APIs.
 */
@VerifiesOnR
@TargetApi(Build.VERSION_CODES.R)
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
}
