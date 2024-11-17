// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.webkit.WebChromeClient;

import org.chromium.blink.mojom.FileChooserParams;

/** This is a helper class to map native file choice mode flags to their correct values. */
public final class FileModeConversionHelper {
    public static int convertFileChooserMode(@FileChooserParams.Mode.EnumType int fileChooserMode) {
        switch (fileChooserMode) {
            case FileChooserParams.Mode.OPEN:
                return WebChromeClient.FileChooserParams.MODE_OPEN;
            case FileChooserParams.Mode.OPEN_MULTIPLE:
            case FileChooserParams.Mode.UPLOAD_FOLDER:
                // UPLOAD_FOLDER in chrome implies multiple.
            case FileChooserParams.Mode.OPEN_DIRECTORY:
                // TODO(crbug.com/40101963): Map OPEN_DIRECTORY to OPEN_MULTIPLE for now, but map to
                // WebChromeClient.FileChooserParams.MODE_OPEN_FOLDER when it is available.
                return WebChromeClient.FileChooserParams.MODE_OPEN_MULTIPLE;
            case FileChooserParams.Mode.SAVE:
                return WebChromeClient.FileChooserParams.MODE_SAVE;
            default:
                assert false : "Unexpected file chooser mode encountered.";
        }
        // default return value should never reach here
        return WebChromeClient.FileChooserParams.MODE_OPEN;
    }

    // Do not instantiate this class.
    private FileModeConversionHelper() {}
}
