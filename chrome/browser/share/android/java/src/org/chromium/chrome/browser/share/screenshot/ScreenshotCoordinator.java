// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.app.Activity;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.share.BaseScreenshotCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/** Handles the screenshot action in the Sharing Hub and launches the screenshot editor. */
public class ScreenshotCoordinator extends BaseScreenshotCoordinator {
    private final WindowAndroid mWindowAndroid;
    private final ScreenshotShareSheetDialog mDialog;

    /**
     * Constructs a new ScreenshotCoordinator which may launch the editor, or a fallback.
     *
     * @param activity The parent activity.
     * @param windowAndroid The {@link WindowAndroid} which contains the content to share.
     * @param shareUrl The URL associated with the screenshot.
     * @param chromeOptionShareCallback An interface to share sheet APIs.
     * @param sheetController The {@link BottomSheetController} for the current activity.
     */
    public ScreenshotCoordinator(
            Activity activity,
            WindowAndroid windowAndroid,
            String shareUrl,
            ChromeOptionShareCallback chromeOptionShareCallback,
            BottomSheetController sheetController) {
        super(activity, shareUrl, chromeOptionShareCallback, sheetController);
        mWindowAndroid = windowAndroid;
        mDialog = new ScreenshotShareSheetDialog();
    }

    /**
     * Constructor for testing.
     *
     * @param activity The parent activity.
     * @param windowAndroid The WindowAndroid which contains the content to share.
     * @param shareUrl The URL associated with the screenshot.
     * @param screenshotSource The screenshot source.
     * @param dialog The Share Sheet dialog to use as fallback.
     * @param chromeOptionShareCallback An interface to share sheet APIs.
     * @param sheetController The {@link BottomSheetController} for the current activity.
     */
    @VisibleForTesting
    ScreenshotCoordinator(
            Activity activity,
            WindowAndroid windowAndroid,
            String shareUrl,
            EditorScreenshotSource screenshotSource,
            ScreenshotShareSheetDialog dialog,
            ChromeOptionShareCallback chromeOptionShareCallback,
            BottomSheetController sheetController) {
        super(activity, shareUrl, chromeOptionShareCallback, sheetController, screenshotSource);
        mWindowAndroid = windowAndroid;
        mDialog = dialog;
    }

    /**
     * Opens the editor with the captured screenshot if the editor is installed. Otherwise, attempts
     * to install the DFM a set amount of times per session. If MAX_INSTALL_ATTEMPTS is reached,
     * directly opens the screenshot sharesheet instead.
     */
    @Override
    protected void handleScreenshot() {
        if (mScreenshot == null) {
            // TODO(crbug.com/40107491): Show error message
            return;
        }

        ScreenshotShareSheetDialogCoordinator shareSheet =
                new ScreenshotShareSheetDialogCoordinator(
                        mActivity,
                        mDialog,
                        mScreenshot,
                        mWindowAndroid,
                        mShareUrl,
                        mChromeOptionShareCallback);
        shareSheet.showShareSheet();
        mScreenshot = null;
    }
}
