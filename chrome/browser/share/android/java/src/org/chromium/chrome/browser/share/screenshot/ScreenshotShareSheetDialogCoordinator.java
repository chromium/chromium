// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.app.Activity;
import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.ui.base.WindowAndroid;

/** Coordinator for displaying the screenshot share sheet dialog. */
public class ScreenshotShareSheetDialogCoordinator {
    private final ScreenshotShareSheetDialog mDialog;
    private final FragmentManager mFragmentManager;

    /**
     * Constructs a new Screenshot Dialog.
     *
     * @param activity The parent activity.
     * @param dialog The Share Sheet dialog to use as fallback.
     * @param screenshot The Bitmap of the screenshot to share.
     * @param windowAndroid The {@link WindowAndroid} which contains the content to share.
     * @param shareUrl The URL associated with the screenshot.
     * @param shareCallback Callback called when falling back to the share sheet.
     */
    public ScreenshotShareSheetDialogCoordinator(
            Activity activity,
            ScreenshotShareSheetDialog dialog,
            Bitmap screenshot,
            WindowAndroid windowAndroid,
            String shareUrl,
            ChromeOptionShareCallback shareCallback) {
        mFragmentManager = ((FragmentActivity) activity).getSupportFragmentManager();
        mDialog = dialog;
        mDialog.init(screenshot, windowAndroid, shareUrl, shareCallback);
    }

    /** Show the main share sheet dialog. */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public void showShareSheet() {
        mDialog.show(mFragmentManager, null);
    }
}
