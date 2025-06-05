// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.webkit.WebChromeClient;

import org.chromium.android_webview.common.AwFeatureMap;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.blink.mojom.FileChooserParams;
import org.chromium.build.annotations.NullMarked;

/** This is a helper class to map native file choice mode flags to their correct values. */
@NullMarked
public final class FileModeConversionHelper {
    public static int convertFileChooserMode(@FileChooserParams.Mode.EnumType int fileChooserMode) {
        switch (fileChooserMode) {
            case FileChooserParams.Mode.OPEN:
                return WebChromeClient.FileChooserParams.MODE_OPEN;
            case FileChooserParams.Mode.OPEN_MULTIPLE:
            case FileChooserParams.Mode.UPLOAD_FOLDER:
                return WebChromeClient.FileChooserParams.MODE_OPEN_MULTIPLE;
            case FileChooserParams.Mode.OPEN_DIRECTORY:
                if (AwFeatureMap.isEnabled(AwFeatures.WEBVIEW_FILE_SYSTEM_ACCESS)) {
                    // TODO(crbug.com/40101963): when default builds use Android B SDK, use
                    // WebChromeClient.FileChooserParams.MODE_OPEN_FOLDER.
                    return AwContentsClient.FileChooserParamsImpl.Mode.OPEN_FOLDER;
                } else {
                    return WebChromeClient.FileChooserParams.MODE_OPEN_MULTIPLE;
                }
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
