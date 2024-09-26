// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.share_tab;

import android.Manifest.permission;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.os.Build;
import android.os.Process;
import android.text.DynamicLayout;
import android.text.Layout.Alignment;
import android.text.TextPaint;
import android.text.TextUtils;
import android.text.TextUtils.TruncateAt;
import android.view.View;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.FileAccessPermissionHelper;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.share.BitmapDownloadRequest;
import org.chromium.chrome.browser.share.qrcode.QRCodeGenerator;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * QrCodeShareMediator is in charge of calculating and setting values for QrCodeShareViewProperties.
 */
class QrCodeShareMediator {
    // QR code version 40 with M-level error correction can encode binary inputs of up to 2331
    // bytes, and digit-only inputs of up to 5596 bytes.  See
    // https://www.qrcode.com/en/about/version.html.
    //
    // See also `kMaxInputLength` in
    // `//chrome/browser/ui/views/qrcode_generator/qrcode_generator_bubble.cc`.
    private static final int MAX_URL_LENGTH = 2331;

    private final Context mContext;
    private final PropertyModel mPropertyModel;
    private WindowAndroid mWindowAndroid;

    // The number of times the user has attempted to download the QR code in this dialog.
    private int mNumDownloads;

    private boolean mIsDownloadInProgress;
    private String mUrl;
    private Runnable mCloseDialog;

    /**
     * The QrCodeScanMediator constructor.
     *
     * @param context The context to use.
     * @param propertyModel The property model to use to communicate with views.
     * @param closeDialog The {@link Runnable} to close the dialog.
     * @param url The url to create the QRCode.
     */
    QrCodeShareMediator(
            Context context,
            PropertyModel propertyModel,
            Runnable closeDialog,
            String url,
            WindowAndroid windowAndroid) {
        mContext = context;
        mPropertyModel = propertyModel;
        mCloseDialog = closeDialog;
        mUrl = url;
        ChromeBrowserInitializer.getInstance()
                .runNowOrAfterFullBrowserStarted(() -> refreshQrCode(mUrl));
        mWindowAndroid = windowAndroid;
        updatePermissionSettings();
    }

    /**
     * Refreshes the QR Code bitmap for given data.
     * @param data The data to encode.
     */
    protected void refreshQrCode(String data) {
        if (TextUtils.isEmpty(data)) {
            mPropertyModel.set(
                    QrCodeShareViewProperties.ERROR_STRING,
                    mContext.getResources().getString(R.string.qr_code_error_unknown));
            return;
        }

        Bitmap bitmap = QRCodeGenerator.generateBitmap(data);
        if (bitmap != null) {
            mPropertyModel.set(QrCodeShareViewProperties.QRCODE_BITMAP, bitmap);
            return;
        }
        String errorMessage;
        if (data != null && data.length() > MAX_URL_LENGTH) {
            errorMessage =
                    mContext.getResources()
                            .getString(R.string.qr_code_error_too_long, MAX_URL_LENGTH);
        } else {
            errorMessage = mContext.getResources().getString(R.string.qr_code_error_unknown);
        }
        mPropertyModel.set(QrCodeShareViewProperties.ERROR_STRING, errorMessage);
    }

    /** Triggers download for the generated QR code bitmap if available. */
    protected void downloadQrCode(View view) {
        logDownload();
        Bitmap qrcodeBitmap = mPropertyModel.get(QrCodeShareViewProperties.QRCODE_BITMAP);
        if (qrcodeBitmap != null && !mIsDownloadInProgress && mWindowAndroid != null) {
            FileAccessPermissionHelper.requestFileAccessPermission(
                    mWindowAndroid, this::finishDownloadWithPermission);
            return;
        }
    }

    private void finishDownloadWithPermission(boolean granted) {
        if (granted) {
            updatePermissionSettings();
            Bitmap qrcodeBitmap = mPropertyModel.get(QrCodeShareViewProperties.QRCODE_BITMAP);
            String fileName =
                    mContext.getString(
                            R.string.qr_code_filename_prefix,
                            String.valueOf(System.currentTimeMillis()));
            mIsDownloadInProgress = true;
            BitmapDownloadRequest.downloadBitmap(fileName, addUrlToBitmap(qrcodeBitmap, mUrl));
            mCloseDialog.run();
        }
    }

    /** Returns whether we need to explicitly request a storage permission. */
    private Boolean requiresAdditionalStoragePermission() {
        return (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU);
    }

    /** Returns whether the user has granted storage permissions. */
    private Boolean hasStoragePermission() {
        // Not needed for newer SDKs; treat as granted implicitly.
        if (!requiresAdditionalStoragePermission()) return true;
        return mContext.checkPermission(
                        permission.WRITE_EXTERNAL_STORAGE, Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }

    /** Returns whether the user can be prompted for storage permissions. */
    private Boolean canPromptForPermission() {
        if (!requiresAdditionalStoragePermission()) return false;
        if (mWindowAndroid == null) return false;
        return mWindowAndroid.canRequestPermission(permission.WRITE_EXTERNAL_STORAGE);
    }

    /** Updates the permission settings with the latest values. */
    private void updatePermissionSettings() {
        mPropertyModel.set(
                QrCodeShareViewProperties.CAN_PROMPT_FOR_PERMISSION, canPromptForPermission());
        mPropertyModel.set(
                QrCodeShareViewProperties.HAS_STORAGE_PERMISSION, hasStoragePermission());
    }

    public void updatePermissions(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
        updatePermissionSettings();
    }

    /**
     * Sets whether QrCode UI is on foreground.
     *
     * @param isOnForeground Indicates whether this component UI is current on foreground.
     */
    public void setIsOnForeground(boolean isOnForeground) {
        // If the app is in the foreground, the permissions need to be checked again to ensure
        // the user is seeing the right thing.
        if (isOnForeground) {
            updatePermissionSettings();
        }
        // This is intentionally done last so that the view is updated according to the latest
        // permissions.
        mPropertyModel.set(QrCodeShareViewProperties.IS_ON_FOREGROUND, isOnForeground);
    }

    /** Logs user actions when attempting to download a QR code. */
    private void logDownload() {
        // Always log the singular metric; otherwise it's easy to miss during analysis.
        RecordUserAction.record("SharingQRCode.DownloadQRCode");
        if (mNumDownloads > 0) {
            RecordUserAction.record("SharingQRCode.DownloadQRCodeMultipleAttempts");
        }
        mNumDownloads++;
    }

    private Bitmap addUrlToBitmap(Bitmap bitmap, String url) {
        int qrCodeSize = mContext.getResources().getDimensionPixelSize(R.dimen.qrcode_size);
        int fontSize = mContext.getResources().getDimensionPixelSize(R.dimen.text_size_large);
        int sidePadding = mContext.getResources().getDimensionPixelSize(R.dimen.side_padding);
        int textTopPadding =
                mContext.getResources().getDimensionPixelSize(R.dimen.url_box_top_padding);
        int textBottomPadding =
                mContext.getResources().getDimensionPixelSize(R.dimen.url_box_bottom_padding);

        TextPaint mTextPaint = new TextPaint();
        mTextPaint.setAntiAlias(true);
        mTextPaint.setColor(android.graphics.Color.BLACK);
        mTextPaint.setTextSize(fontSize);

        // Text is as wide as the QR code.
        FixedLineCountLayout mTextLayout =
                new FixedLineCountLayout(
                        url, mTextPaint, qrCodeSize, Alignment.ALIGN_CENTER, 1.0f, 0.0f, true, 2);

        // New bitmap should be long enough to fit the url with its margins, the QR code bitmap and
        // equal padding from the bottom.
        int height =
                (textTopPadding + mTextLayout.getHeight() + textBottomPadding) * 2 + qrCodeSize;
        int width = qrCodeSize + 2 * sidePadding;
        Bitmap newBitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(newBitmap);
        canvas.drawColor(android.graphics.Color.WHITE);
        canvas.translate(sidePadding, textTopPadding);
        mTextLayout.draw(canvas);
        canvas.drawBitmap(
                Bitmap.createScaledBitmap(bitmap, qrCodeSize, qrCodeSize, false),
                0,
                mTextLayout.getHeight() + textBottomPadding,
                mTextPaint);
        return newBitmap;
    }

    // Helps to limit number of text lines and shows ellipsis for only the last line.
    static class FixedLineCountLayout extends DynamicLayout {
        int mMaxLines;

        FixedLineCountLayout(
                CharSequence base,
                TextPaint paint,
                int width,
                Alignment align,
                float spacingmult,
                float spacingadd,
                boolean includepad,
                int maxLines) {
            super(
                    base,
                    base,
                    paint,
                    width,
                    align,
                    spacingmult,
                    spacingadd,
                    includepad,
                    TruncateAt.END,
                    width);
            mMaxLines = maxLines;
        }

        @Override
        public int getLineCount() {
            if (super.getLineCount() - 1 > mMaxLines) {
                return mMaxLines;
            }
            return super.getLineCount() - 1;
        }

        @Override
        public int getEllipsisCount(int line) {
            if (line == mMaxLines - 1 && super.getLineCount() - 2 > line) {
                return 3;
            }
            return 0;
        }

        @Override
        public int getEllipsisStart(int line) {
            if (line == mMaxLines - 1 && super.getLineCount() - 2 > line) {
                return getLineEnd(line) - getLineStart(line) - 1;
            }
            return 0;
        }
    }
}
