// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import android.app.Activity;
import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.paint_preview.PaintPreviewCompositorUtils;
import org.chromium.chrome.browser.share.screenshot.ScreenshotCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.modules.image_editor.ImageEditorModuleProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * Handles the long screenshot action in the Sharing Hub and launches the screenshot editor.
 */
public class LongScreenshotsCoordinator extends ScreenshotCoordinator {
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

        PaintPreviewCompositorUtils.warmupCompositor();
    }

    /**
     * Called after ShareSheetBottomSheetContent is closed. Calls the FDT service to generate a long
     * screenshot, takes the user through the cropping flow, then launches the bottom bar.
     */
    @Override
    public void captureScreenshot() {
        EntryManager entryManager = new EntryManager(mActivity, mTab);

        entryManager.generateInitialBitmap(new Callback<Bitmap>() {
            @Override
            public void onResult(Bitmap result) {
                mScreenshot = result;
                launchSharesheet();
            }
        });
    }
}
