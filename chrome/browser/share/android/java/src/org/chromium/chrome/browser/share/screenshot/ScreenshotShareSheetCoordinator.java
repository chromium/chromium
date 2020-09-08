// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.ActivityAndroidPermissionDelegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;

/**
 * Coordinator for displaying the screenshot share sheet.
 */
public class ScreenshotShareSheetCoordinator {
    private final ScreenshotShareSheetSaveDelegate mSaveDelegate;
    private final ScreenshotShareSheetMediator mMediator;
    private final PropertyModel mModel;

    /**
     * Constructs a new ShareSheetCoordinator.
     *
     * @param context The context to use for user permissions.
     * @param screenshot The screenshot to be shared.
     * @param deleteRunanble The runnable to be called on cancel or delete.
     * @param screenshotShareSheetView the view for the screenshot share sheet.
     * @param tab The tab that launched this screenshot.
     * @param shareSheetCallback The callback to be called on share.
     * @param installCallback The callback to be called on retry.
     */
    public ScreenshotShareSheetCoordinator(Context context, Bitmap screenshot,
            Runnable closeDialogRunnable, ScreenshotShareSheetView screenshotShareSheetView,
            Tab tab, ChromeOptionShareCallback shareSheetCallback,
            Callback<Runnable> installCallback) {
        ArrayList<PropertyKey> allProperties =
                new ArrayList<>(Arrays.asList(ScreenshotShareSheetViewProperties.ALL_KEYS));
        mModel = new PropertyModel(allProperties);

        mModel.set(ScreenshotShareSheetViewProperties.SCREENSHOT_BITMAP, screenshot);
        mSaveDelegate = new ScreenshotShareSheetSaveDelegate(context, mModel, closeDialogRunnable,
                new ActivityAndroidPermissionDelegate(
                        new WeakReference<Activity>((Activity) context)));
        mMediator = new ScreenshotShareSheetMediator(context, mModel, closeDialogRunnable,
                mSaveDelegate::save, tab, shareSheetCallback, installCallback);

        PropertyModelChangeProcessor.create(
                mModel, screenshotShareSheetView, ScreenshotShareSheetViewBinder::bind);
    }
}
