// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.Manifest.permission;
import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.provider.Settings;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.FileAccessPermissionHelper;
import org.chromium.ui.base.WindowAndroid;

import java.text.DateFormat;
import java.util.Date;

/** SaveBitmapDelegate is in charge of download the current bitmap. */
public class SaveBitmapDelegate {
    private final Context mContext;
    private final Bitmap mBitmap;
    private final int mFileNameResource;
    private final WindowAndroid mWindowAndroid;
    private final Runnable mCallback;
    private Dialog mDialog;

    /**
     * The SaveBitmapDelegate constructor.
     *
     * @param context The context to use.
     * @param bitmap The bitmap to save.
     * @param fileNameResource The file name resource id to use when saving the image.
     * @param callback The callback to run when download is complete.
     */
    public SaveBitmapDelegate(
            Context context,
            Bitmap bitmap,
            int fileNameResource,
            Runnable callback,
            WindowAndroid windowAndroid) {
        mContext = context;
        mBitmap = bitmap;
        mFileNameResource = fileNameResource;
        mWindowAndroid = windowAndroid;
        mCallback = callback;
    }

    /** Saves the given bitmap. */
    public void save() {
        if (mBitmap == null) {
            return;
        }

        boolean needPermissionRequestButBlocked =
                (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU)
                        && !mWindowAndroid.hasPermission(permission.WRITE_EXTERNAL_STORAGE)
                        && !mWindowAndroid.canRequestPermission(permission.WRITE_EXTERNAL_STORAGE);
        if (needPermissionRequestButBlocked) {
            AlertDialog.Builder builder =
                    new AlertDialog.Builder(mContext, R.style.ThemeOverlay_BrowserUI_AlertDialog);
            builder.setMessage(R.string.sharing_hub_storage_disabled_text)
                    .setNegativeButton(
                            R.string.cancel,
                            new DialogInterface.OnClickListener() {
                                @Override
                                public void onClick(DialogInterface dialog, int which) {
                                    mDialog.cancel();
                                }
                            })
                    .setPositiveButton(
                            R.string.sharing_hub_open_settings_label,
                            new DialogInterface.OnClickListener() {
                                @Override
                                public void onClick(DialogInterface dialog, int which) {
                                    Intent intent =
                                            new Intent(
                                                    Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
                                    intent.setData(
                                            new Uri.Builder()
                                                    .scheme("package")
                                                    .opaquePart(mContext.getPackageName())
                                                    .build());
                                    ((Activity) mContext).startActivity(intent);
                                }
                            });
            mDialog = builder.create();
            mDialog.setCanceledOnTouchOutside(false);
            mDialog.show();
            return;
        }

        FileAccessPermissionHelper.requestFileAccessPermission(
                mWindowAndroid, this::finishDownloadWithPermission);
    }

    @VisibleForTesting
    protected void finishDownloadWithPermission(boolean granted) {
        if (granted) {
            DateFormat dateFormat =
                    DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.LONG);
            String fileName =
                    mContext.getString(
                            mFileNameResource,
                            dateFormat.format(new Date(System.currentTimeMillis())));
            BitmapDownloadRequest.downloadBitmap(fileName, mBitmap);

            if (mCallback != null) mCallback.run();
        }
    }

    // Used in tests.
    Dialog getDialog() {
        return mDialog;
    }
}
