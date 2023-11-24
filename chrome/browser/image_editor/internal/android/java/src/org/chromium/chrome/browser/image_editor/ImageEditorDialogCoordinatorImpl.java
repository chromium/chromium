// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_editor;

import android.app.Activity;
import android.graphics.Bitmap;

import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.ui.base.WindowAndroid;

/**
 * Upstream implementation for ImageEditorDialogCoordinator. Does nothing. Actual implementation
 * lives downstream.
 */
public class ImageEditorDialogCoordinatorImpl implements ImageEditorDialogCoordinator {
    @Override
    public void launchEditor(
            Activity activity,
            Bitmap image,
            WindowAndroid windowAndroid,
            ChromeOptionShareCallback chromeOptionShareCallback) {}
}
