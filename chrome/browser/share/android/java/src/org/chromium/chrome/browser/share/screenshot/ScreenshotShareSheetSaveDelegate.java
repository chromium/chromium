// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.Manifest.permission;
import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.provider.Settings;

import androidx.appcompat.app.AlertDialog;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.download.DownloadController;
import org.chromium.chrome.browser.share.BitmapDownloadRequest;
import org.chromium.ui.base.AndroidPermissionDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.text.DateFormat;
import java.util.Date;

/**
 * ScreenshotShareSheetSaveDelegate is in charge of download the current bitmap.
 */
class ScreenshotShareSheetSaveDelegate {
    private final PropertyModel mModel;
    private final Context mContext;
    private final AndroidPermissionDelegate mPermissionDelegate;
    private final Runnable mCloseDialogRunnable;
    private Bitmap mBitmap;
    private Dialog mDialog;

    /**
     * The ScreenshotShareSheetSaveDelegate constructor.
     * @param context The context to use.
     * @param propertyModel The property model to use to communicate with views.
     */
    ScreenshotShareSheetSaveDelegate(Context context, PropertyModel propertyModel,
            Runnable closeDialogRunnable, AndroidPermissionDelegate permissionDelegate) {
        mContext = context;
        mModel = propertyModel;
        mPermissionDelegate = permissionDelegate;
        mCloseDialogRunnable = closeDialogRunnable;
    }

    /**
     * Saves the current image.
     */
    protected void save() {
        mBitmap = mModel.get(ScreenshotShareSheetViewProperties.SCREENSHOT_BITMAP);
        if (mBitmap == null) {
            return;
        }

        if (!mPermissionDelegate.hasPermission(permission.WRITE_EXTERNAL_STORAGE)
                && !mPermissionDelegate.canRequestPermission(permission.WRITE_EXTERNAL_STORAGE)) {
            AlertDialog.Builder builder =
                    new AlertDialog.Builder(mContext, R.style.Theme_Chromium_AlertDialog);
            builder.setMessage(R.string.sharing_hub_storage_disabled_text)
                    .setNegativeButton(R.string.cancel,
                            new DialogInterface.OnClickListener() {
                                @Override
                                public void onClick(DialogInterface dialog, int which) {
                                    mDialog.cancel();
                                }
                            })
                    .setPositiveButton(R.string.sharing_hub_open_settings_label,
                            new DialogInterface.OnClickListener() {
                                @Override
                                public void onClick(DialogInterface dialog, int which) {
                                    Intent intent = new Intent(
                                            Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
                                    intent.setData(new Uri.Builder()
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

        DownloadController.requestFileAccessPermission(this::finishDownloadWithPermission);
    }

    private void finishDownloadWithPermission(boolean granted) {
        if (granted) {
            DateFormat dateFormat =
                    DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.LONG);
            String fileName = mContext.getString(R.string.screenshot_filename_prefix,
                    dateFormat.format(new Date(System.currentTimeMillis())));
            BitmapDownloadRequest.downloadBitmap(fileName, mBitmap);
            mCloseDialogRunnable.run();
        }
    }

    Dialog getDialog() {
        return mDialog;
    }
}
