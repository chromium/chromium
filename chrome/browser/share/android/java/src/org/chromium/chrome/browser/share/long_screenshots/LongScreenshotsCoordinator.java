// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.paint_preview.PaintPreviewCompositorUtils;
import org.chromium.chrome.browser.share.long_screenshots.bitmap_generation.EntryManager;
import org.chromium.chrome.browser.share.screenshot.ScreenshotCoordinator;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.widget.Toast;

/** Handles the long screenshot action in the Sharing Hub and launches the screenshot editor. */
public class LongScreenshotsCoordinator extends ScreenshotCoordinator {
    private final Activity mActivity;
    private final EntryManager mEntryManager;
    private final Tab mTab;
    private LongScreenshotsMediator mMediator;

    /**
     * Private internal method to construct a LongScreenshotsCoordinator. Other users of this class
     * should instead you LongScreenshotsCoordinator.create(...).
     *
     * @param activity The parent activity.
     * @param tab The Tab which contains the content to share.
     * @param shareUrl The URL associated with the screenshot.
     * @param chromeOptionShareCallback An interface to share sheet APIs.
     * @param sheetController The {@link BottomSheetController} for the current activity.
     * @param manager The {@link EntryManager} to retrieve bitmaps of the current tab.
     * @param mediator The {@link LongScreenshotsMediator} The mediator that controls the long
     * screenshots dialog behavior.
     * @param shouldWarmupCompositor If the PaintPreview compositor should be warmed up.
     */
    private LongScreenshotsCoordinator(
            Activity activity,
            Tab tab,
            String shareUrl,
            ChromeOptionShareCallback chromeOptionShareCallback,
            BottomSheetController sheetController,
            EntryManager manager,
            @Nullable LongScreenshotsMediator mediator,
            boolean shouldWarmupCompositor) {
        super(
                activity,
                tab.getWindowAndroid(),
                shareUrl,
                chromeOptionShareCallback,
                sheetController);
        mActivity = activity;
        mTab = tab;
        mEntryManager =
                manager == null
                        ? new EntryManager(mActivity, mTab, /* inMemory= */ false)
                        : manager;
        mMediator = mediator;

        if (shouldWarmupCompositor) {
            PaintPreviewCompositorUtils.warmupCompositor();
        }
    }

    /** Public interface used to create a {@link LongScreenshotsCoordinator}. */
    public static LongScreenshotsCoordinator create(
            Activity activity,
            Tab tab,
            String shareUrl,
            ChromeOptionShareCallback chromeOptionShareCallback,
            BottomSheetController sheetController) {
        return new LongScreenshotsCoordinator(
                activity,
                tab,
                shareUrl,
                chromeOptionShareCallback,
                sheetController,
                null,
                null,
                true);
    }

    /** Called by tests to create a {@link LongScreenshotsCoordinator}. */
    public static LongScreenshotsCoordinator createForTests(
            Activity activity,
            Tab tab,
            String shareUrl,
            ChromeOptionShareCallback chromeOptionShareCallback,
            BottomSheetController sheetController,
            EntryManager manager,
            LongScreenshotsMediator mediator) {
        return new LongScreenshotsCoordinator(
                activity,
                tab,
                shareUrl,
                chromeOptionShareCallback,
                sheetController,
                manager,
                mediator,
                false);
    }

    /**
     * Called after ShareSheetBottomSheetContent is closed. Calls the FDT service to generate a long
     * screenshot, takes the user through the cropping flow, then launches the bottom bar.
     */
    @Override
    public void captureScreenshot() {
        if (mMediator == null) {
            mMediator = new LongScreenshotsMediator(mActivity, mEntryManager);
        }
        mMediator.capture(
                () -> {
                    mScreenshot = mMediator.getScreenshot();
                    if (mScreenshot == null) {
                        Toast.makeText(
                                        mActivity,
                                        R.string.sharing_long_screenshot_unknown_error,
                                        Toast.LENGTH_LONG)
                                .show();
                    } else {
                        super.handleScreenshot();
                    }
                });
    }
}
