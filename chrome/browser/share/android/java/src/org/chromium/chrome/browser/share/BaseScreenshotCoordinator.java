// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;
import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.share.screenshot.EditorScreenshotSource;
import org.chromium.chrome.browser.share.screenshot.EditorScreenshotTask;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;

/**
 * Base coordinator for Sharing Hub features that require taking a browser screenshot. Handles
 * taking the screenshot after the bottom sheet is dismissed.
 */
public abstract class BaseScreenshotCoordinator implements BottomSheetObserver {
    protected final Activity mActivity;
    protected final String mShareUrl;
    protected final ChromeOptionShareCallback mChromeOptionShareCallback;
    protected final BottomSheetController mBottomSheetController;

    private final EditorScreenshotSource mScreenshotSource;
    protected Bitmap mScreenshot;

    /**
     * Constructs a new BaseScreenshotCoordinator that takes a screenshot when the content of the
     * {@link BottomSheet} is hidden.
     *
     * @param activity The parent activity.
     * @param shareUrl The URL associated with the screenshot.
     * @param chromeOptionShareCallback An interface to share sheet APIs.
     * @param sheetController The {@link BottomSheetController} for the current activity.
     */
    public BaseScreenshotCoordinator(
            Activity activity,
            String shareUrl,
            ChromeOptionShareCallback chromeOptionShareCallback,
            BottomSheetController sheetController) {
        this(
                activity,
                shareUrl,
                chromeOptionShareCallback,
                sheetController,
                new EditorScreenshotTask(activity, sheetController));
    }

    /**
     * Constructor for testing and inner construction.
     *
     * @param activity The parent activity.
     * @param shareUrl The URL associated with the screenshot.
     * @param chromeOptionShareCallback An interface to share sheet APIs.
     * @param sheetController The {@link BottomSheetController} for the current activity.
     * @param screenshotSource The Source interface to use to take a screenshot.
     */
    @VisibleForTesting
    protected BaseScreenshotCoordinator(
            Activity activity,
            String shareUrl,
            ChromeOptionShareCallback chromeOptionShareCallback,
            BottomSheetController sheetController,
            EditorScreenshotSource screenshotSource) {
        mActivity = activity;
        mShareUrl = shareUrl;
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mBottomSheetController = sheetController;
        mScreenshotSource = screenshotSource;
    }

    /** Takes a screenshot of the current tab and invokes the handler method. */
    public void captureScreenshot() {
        mScreenshotSource.capture(
                () -> {
                    mScreenshot = mScreenshotSource.getScreenshot();
                    handleScreenshot();
                });
    }

    /** Invoked when the screenshot data is received. To be overridden by subclasses. */
    protected abstract void handleScreenshot();

    /** BottomSheetObserver implementation. */
    @Override
    public void onSheetOpened(int reason) {}

    @Override
    public void onSheetClosed(int reason) {}

    @Override
    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {}

    @Override
    public void onSheetStateChanged(int newState, int reason) {
        if (newState == SheetState.HIDDEN) {
            // Clean up the observer since the coordinator is discarded when the sheet is hidden.
            mBottomSheetController.removeObserver(this);
            captureScreenshot();
        }
    }

    @Override
    public void onSheetContentChanged(BottomSheetContent newContent) {}
}
