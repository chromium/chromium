// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.image_editor;

import android.app.Activity;
import android.graphics.Bitmap;

import org.chromium.base.annotations.DoNotInline;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Interface to interact with the image editor dialog.
 */
// TODO(crbug.com/1145287): DexSplitter has issues with interfaces that have a single implementer
// inside a DFM.
@DoNotInline
public interface ImageEditorDialogCoordinator {
    public void launchEditor(Activity activity, Bitmap image, Tab mTab,
            ChromeOptionShareCallback chromeOptionShareCallback);
}
