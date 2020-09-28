// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.screenshot.ScreenshotShareSheetViewProperties.NoArgOperation;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Date;
import java.util.Locale;

/**
 * ScreenshotShareSheetMediator is in charge of calculating and setting values for
 * ScreenshotShareSheetViewProperties.
 */
class ScreenshotShareSheetMediator {
    private static final String sIsoDateFormat = "yyyy-MM-dd";

    private final PropertyModel mModel;
    private final Context mContext;
    private final Runnable mSaveRunnable;
    private final Runnable mCloseDialogRunnable;
    private final Callback<Runnable> mInstallCallback;
    private final ChromeOptionShareCallback mChromeOptionShareCallback;

    private final Tab mTab;

    /**
     * The ScreenshotShareSheetMediator constructor.
     * @param context The context to use.
     * @param propertyModel The property model to use to communicate with views.
     * @param closeDialogRunnable The action to take to close the dialog.
     * @param saveRunnable The action to take when save is called.
     * @param tab The tab that originated this screenshot.
     * @param chromeOptionShareCallback The callback to share a screenshot via the share sheet.
     * @param installCallback The action to take when install is called, will call runnable on
     *         success.
     */
    ScreenshotShareSheetMediator(Context context, PropertyModel propertyModel,
            Runnable closeDialogRunnable, Runnable saveRunnable, Tab tab,
            ChromeOptionShareCallback chromeOptionShareCallback,
            Callback<Runnable> installCallback) {
        mCloseDialogRunnable = closeDialogRunnable;
        mSaveRunnable = saveRunnable;
        mContext = context;
        mModel = propertyModel;
        mTab = tab;
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mInstallCallback = installCallback;
        mModel.set(ScreenshotShareSheetViewProperties.NO_ARG_OPERATION_LISTENER,
                operation -> { performNoArgOperation(operation); });
    }

    /**
     * Performs the operation passed in.
     *
     * @param operation The operation to perform.
     */
    public void performNoArgOperation(
            @ScreenshotShareSheetViewProperties.NoArgOperation int operation) {
        if (NoArgOperation.SHARE == operation) {
            share();
        } else if (NoArgOperation.SAVE == operation) {
            mSaveRunnable.run();
        } else if (NoArgOperation.DELETE == operation) {
            mCloseDialogRunnable.run();
        } else if (NoArgOperation.INSTALL == operation) {
            mInstallCallback.onResult(mCloseDialogRunnable);
        }
    }

    /**
     * Sends the current image to the share target.
     */
    private void share() {
        if (!mTab.isInitialized()) {
            return;
        }
        Bitmap bitmap = mModel.get(ScreenshotShareSheetViewProperties.SCREENSHOT_BITMAP);

        WindowAndroid window = mTab.getWindowAndroid();
        String isoDate = new SimpleDateFormat(sIsoDateFormat, Locale.getDefault())
                                 .format(new Date(System.currentTimeMillis()));
        String title = mContext.getString(R.string.screenshot_title_for_share, isoDate);
        Callback<Uri> callback = (bitmapUri) -> {
            ShareParams params =
                    new ShareParams.Builder(window, title, /*url=*/"")
                            .setFileUris(new ArrayList<>(Collections.singletonList(bitmapUri)))
                            .setFileContentType(
                                    window.getApplicationContext().getContentResolver().getType(
                                            bitmapUri))
                            .build();

            mChromeOptionShareCallback.showThirdPartyShareSheet(
                    params, new ChromeShareExtras.Builder().build(), System.currentTimeMillis());
        };

        generateTemporaryUriFromBitmap(mContext, title, bitmap, callback);
        mCloseDialogRunnable.run();
    }

    protected void generateTemporaryUriFromBitmap(
            Context context, String fileName, Bitmap bitmap, Callback<Uri> callback) {
        ShareImageFileUtils.generateTemporaryUriFromBitmap(context, fileName, bitmap, callback);
    }
}
