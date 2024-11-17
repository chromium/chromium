// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.content.Context;
import android.graphics.Bitmap;

import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.Arrays;

/** Coordinator for displaying the screenshot share sheet. */
public class ScreenshotShareSheetCoordinator {
    private final ScreenshotShareSheetSaveDelegate mSaveDelegate;
    private final PropertyModel mModel;

    /**
     * Constructs a new ShareSheetCoordinator.
     *
     * @param context The context to use for user permissions.
     * @param screenshot The screenshot to be shared.
     * @param closeDialogRunnable The runnable to be called on cancel or delete.
     * @param screenshotShareSheetView the view for the screenshot share sheet.
     * @param windowAndroid The {@link WindowAndroid} that launched this screenshot.
     * @param shareUrl The URL associated with the screenshot.
     * @param shareSheetCallback The callback to be called on share.
     */
    public ScreenshotShareSheetCoordinator(
            Context context,
            Bitmap screenshot,
            Runnable closeDialogRunnable,
            ScreenshotShareSheetView screenshotShareSheetView,
            WindowAndroid windowAndroid,
            String shareUrl,
            ChromeOptionShareCallback shareSheetCallback) {
        ArrayList<PropertyKey> allProperties =
                new ArrayList<>(Arrays.asList(ScreenshotShareSheetViewProperties.ALL_KEYS));
        mModel = new PropertyModel(allProperties);

        mModel.set(ScreenshotShareSheetViewProperties.SCREENSHOT_BITMAP, screenshot);
        mSaveDelegate =
                new ScreenshotShareSheetSaveDelegate(
                        context, mModel, closeDialogRunnable, windowAndroid);
        new ScreenshotShareSheetMediator(
                context,
                mModel,
                closeDialogRunnable,
                mSaveDelegate::save,
                windowAndroid,
                shareUrl,
                shareSheetCallback);

        PropertyModelChangeProcessor.create(
                mModel, screenshotShareSheetView, ScreenshotShareSheetViewBinder::bind);
    }
}
