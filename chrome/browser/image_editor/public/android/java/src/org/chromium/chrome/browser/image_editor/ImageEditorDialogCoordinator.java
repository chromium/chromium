// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_editor;

import android.app.Activity;
import android.graphics.Bitmap;

import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.ui.base.WindowAndroid;

/** Interface to interact with the image editor dialog. */
public interface ImageEditorDialogCoordinator {
    void launchEditor(
            Activity activity,
            Bitmap image,
            WindowAndroid windowAndroid,
            String shareUrl,
            ChromeOptionShareCallback chromeOptionShareCallback);
}
