// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.webkit.WebChromeClient;

import org.chromium.blink.mojom.FileChooserParams;

/** This is a helper class to map native file choice mode flags to their correct values. */
public final class FileModeConversionHelper {
    public static int convertFileChooserMode(int fileChooserMode) {
        switch (fileChooserMode) {
            case FileChooserParams.Mode.OPEN:
                return WebChromeClient.FileChooserParams.MODE_OPEN;
            case FileChooserParams.Mode.OPEN_MULTIPLE:
            case FileChooserParams.Mode.UPLOAD_FOLDER:
                return WebChromeClient.FileChooserParams.MODE_OPEN_MULTIPLE;
            case FileChooserParams.Mode.SAVE:
                assert false : "Save file chooser mode deprecated.";
                // fall through
            default:
                assert false : "Unexpected file chooser mode encountered.";
        }
        return 0; // default return value, should never reach here
    }

    // Do not instantiate this class.
    private FileModeConversionHelper() {}
}
