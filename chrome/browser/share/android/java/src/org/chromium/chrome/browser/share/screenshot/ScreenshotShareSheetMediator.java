// Copyright 2020 The Chromium Authors
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
import org.chromium.components.browser_ui.share.ShareImageFileUtils;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.text.SimpleDateFormat;
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
    private final ChromeOptionShareCallback mChromeOptionShareCallback;
    private final WindowAndroid mWindowAndroid;
    private final String mShareUrl;

    /**
     * The ScreenshotShareSheetMediator constructor.
     *
     * @param context The context to use.
     * @param propertyModel The property model to use to communicate with views.
     * @param closeDialogRunnable The action to take to close the dialog.
     * @param saveRunnable The action to take when save is called.
     * @param windowAndroid The {@link WindowAndroid} that originated this screenshot.
     * @param shareUrl The URL associated with the screenshot.
     * @param chromeOptionShareCallback The callback to share a screenshot via the share sheet.
     */
    ScreenshotShareSheetMediator(
            Context context,
            PropertyModel propertyModel,
            Runnable closeDialogRunnable,
            Runnable saveRunnable,
            WindowAndroid windowAndroid,
            String shareUrl,
            ChromeOptionShareCallback chromeOptionShareCallback) {
        mCloseDialogRunnable = closeDialogRunnable;
        mSaveRunnable = saveRunnable;
        mContext = context;
        mModel = propertyModel;
        mWindowAndroid = windowAndroid;
        mShareUrl = shareUrl;
        mChromeOptionShareCallback = chromeOptionShareCallback;
        mModel.set(
                ScreenshotShareSheetViewProperties.NO_ARG_OPERATION_LISTENER,
                operation -> {
                    performNoArgOperation(operation);
                });
    }

    /**
     * Performs the operation passed in.
     *
     * @param operation The operation to perform.
     */
    public void performNoArgOperation(
            @ScreenshotShareSheetViewProperties.NoArgOperation int operation) {
        if (NoArgOperation.SHARE == operation) {
            ScreenshotShareSheetMetrics.logScreenshotAction(
                    ScreenshotShareSheetMetrics.ScreenshotShareSheetAction.SHARE);
            share();
        } else if (NoArgOperation.SAVE == operation) {
            ScreenshotShareSheetMetrics.logScreenshotAction(
                    ScreenshotShareSheetMetrics.ScreenshotShareSheetAction.SAVE);
            mSaveRunnable.run();
        } else if (NoArgOperation.DELETE == operation) {
            ScreenshotShareSheetMetrics.logScreenshotAction(
                    ScreenshotShareSheetMetrics.ScreenshotShareSheetAction.DELETE);
            mCloseDialogRunnable.run();
        }
    }

    /** Sends the current image to the share target. */
    private void share() {
        Bitmap bitmap = mModel.get(ScreenshotShareSheetViewProperties.SCREENSHOT_BITMAP);

        String isoDate =
                new SimpleDateFormat(sIsoDateFormat, Locale.getDefault())
                        .format(new Date(System.currentTimeMillis()));
        String title = mContext.getString(R.string.screenshot_title_for_share, isoDate);
        Callback<Uri> callback =
                (bitmapUri) -> {
                    ShareParams params =
                            new ShareParams.Builder(mWindowAndroid, title, /* url= */ "")
                                    .setSingleImageUri(bitmapUri)
                                    .setFileContentType(
                                            mWindowAndroid
                                                    .getApplicationContext()
                                                    .getContentResolver()
                                                    .getType(bitmapUri))
                                    .build();

                    mChromeOptionShareCallback.showThirdPartyShareSheet(
                            params,
                            new ChromeShareExtras.Builder()
                                    .setContentUrl(new GURL(mShareUrl))
                                    .setDetailedContentType(
                                            ChromeShareExtras.DetailedContentType.SCREENSHOT)
                                    .build(),
                            System.currentTimeMillis());
                };

        generateTemporaryUriFromBitmap(title, bitmap, callback);
        mCloseDialogRunnable.run();
    }

    protected void generateTemporaryUriFromBitmap(
            String fileName, Bitmap bitmap, Callback<Uri> callback) {
        ShareImageFileUtils.generateTemporaryUriFromBitmap(fileName, bitmap, callback);
    }
}
