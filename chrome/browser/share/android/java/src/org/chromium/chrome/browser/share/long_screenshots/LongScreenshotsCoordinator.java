// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Rect;

import org.chromium.chrome.browser.share.screenshot.ScreenshotCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.modules.image_editor.ImageEditorModuleProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.paint_preview.common.proto.PaintPreview.PaintPreviewProto;

/**
 * Handles the long screenshot action in the Sharing Hub and launches the screenshot editor.
 */
public class LongScreenshotsCoordinator
        extends ScreenshotCoordinator implements LongScreenshotsTabService.CaptureProcessor {
    private LongScreenshotsTabService mLongScreenshotsTabService;

    private static final String DIR_NAME = "long_screenshots_dir";

    /**
     * Constructs a new ScreenshotCoordinator which may launch the editor, or a fallback.
     *
     * @param activity The parent activity.
     * @param tab The Tab which contains the content to share.
     * @param chromeOptionShareCallback An interface to share sheet APIs.
     * @param sheetController The {@link BottomSheetController} for the current activity.
     * @param imageEditorModuleProvider An interface to install and/or instantiate the image editor.
     */
    public LongScreenshotsCoordinator(Activity activity, Tab tab,
            ChromeOptionShareCallback chromeOptionShareCallback,
            BottomSheetController sheetController,
            ImageEditorModuleProvider imageEditorModuleProvider) {
        super(activity, tab, chromeOptionShareCallback, sheetController, imageEditorModuleProvider);

        mLongScreenshotsTabService = LongScreenshotsTabServiceFactory.getServiceInstance();
        mLongScreenshotsTabService.setCaptureProcessor(this);
    }

    /**
     * Called after ShareSheetBottomSheetContent is closed. Calls the FDT service to generate a long
     * screenshot, takes the user through the cropping flow, then launches the bottom bar.
     */
    @Override
    public void captureScreenshot() {
        // TODO(tgupta): Provide the correct bounds.
        mLongScreenshotsTabService.captureTab(mTab, new Rect(0, 0, 1000, 1000));
    }

    /**
     * Called after the bitmap has been composited and can be shown to the user.
     * @param result Bitmap to display in the dialog.
     */
    public void onBitmapResult(Bitmap result) {
        mScreenshot = result;
        launchSharesheet();
    }

    /**
     * Pass the proto response from the LongScreenshotsTabService to the compositor. Called from
     * the tabService.
     *
     * @response PaintPreview response with the address of the stored screenshot
     * @status Status of the capturing
     */
    @Override
    public void processCapturedTab(PaintPreviewProto response, int status) {
        // TODO(tgupta): Process a non success status
        new LongScreenshotsCompositor(mTab, mActivity,
                LongScreenshotsTabServiceFactory.getServiceInstance(), DIR_NAME, response,
                this::onBitmapResult);
    }
}
