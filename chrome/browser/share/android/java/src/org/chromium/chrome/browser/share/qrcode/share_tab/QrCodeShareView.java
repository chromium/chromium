// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.share_tab;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.provider.Settings;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.ui.widget.ChromeImageView;

/** Manages the Android View representing the QrCode share panel. */
class QrCodeShareView {
    private final Context mContext;
    private final View mView;

    private boolean mHasStoragePermission;
    private boolean mCanPromptForPermission;
    private boolean mIsOnForeground;

    public QrCodeShareView(Context context, View.OnClickListener listener) {
        mContext = context;

        mView =
                (View)
                        LayoutInflater.from(context)
                                .inflate(R.layout.qrcode_share_layout, null, false);

        Button downloadButton = (Button) mView.findViewById(R.id.download);
        downloadButton.setOnClickListener(listener);
        Button settingsButton = (Button) mView.findViewById(R.id.settings);
        settingsButton.setOnClickListener(
                new OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        Intent openSettingsIntent = getAppInfoIntent(mContext.getPackageName());
                        ((Activity) mContext).startActivity(openSettingsIntent);
                    }
                });
        updateView();
    }

    public View getView() {
        return mView;
    }

    /**
     * Updates QR code image on share panel.
     *
     * @param bitmap The {@link Bitmap} to display on share panel.
     */
    public void updateQrCodeBitmap(Bitmap bitmap) {
        ChromeImageView qrcodeImageView = mView.findViewById(R.id.qrcode);
        Drawable drawable = new BitmapDrawable(mContext.getResources(), bitmap);
        qrcodeImageView.setImageDrawable(drawable);

        mView.findViewById(R.id.download).setEnabled(true);
        mView.findViewById(R.id.settings).setEnabled(true);
    }

    /**
     * Displays an error message on the share panel.
     *
     * @param errorMessage The error message to display.
     */
    public void displayErrorMessage(String errorMessage) {
        TextView errorView = mView.findViewById(R.id.error_message);
        errorView.setText(errorMessage);
        errorView.setVisibility(View.VISIBLE);

        mView.findViewById(R.id.download).setEnabled(false);
        mView.findViewById(R.id.settings).setEnabled(false);
    }

    public void storagePermissionsChanged(Boolean hasStoragePermission) {
        if (mHasStoragePermission && hasStoragePermission) {
            return;
        }
        mHasStoragePermission = hasStoragePermission;
        updateView();
    }

    public void canPromptForPermissionChanged(Boolean canPromptForPermission) {
        mCanPromptForPermission = canPromptForPermission;
        updateView();
    }

    /**
     * Applies changes necessary changes to the available button.
     *
     * @param isOnForeground Indicates whether this component UI is currently on foreground.
     */
    public void onForegroundChanged(Boolean isOnForeground) {
        mIsOnForeground = isOnForeground;
        if (mIsOnForeground) {
            updateView();
        }
    }

    private void updateView() {
        if (!mIsOnForeground) {
            return;
        }

        Button downloadButton = (Button) mView.findViewById(R.id.download);
        Button settingsButton = (Button) mView.findViewById(R.id.settings);
        if (mHasStoragePermission) {
            downloadButton.setVisibility(View.VISIBLE);
            settingsButton.setVisibility(View.GONE);
        } else if (mCanPromptForPermission) {
            downloadButton.setVisibility(View.VISIBLE);
            settingsButton.setVisibility(View.GONE);
        } else {
            downloadButton.setVisibility(View.GONE);
            settingsButton.setVisibility(View.VISIBLE);
        }
    }

    /** Returns an Intent to show the App Info page for the current app. */
    private Intent getAppInfoIntent(String packageName) {
        Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
        intent.setData(new Uri.Builder().scheme("package").opaquePart(packageName).build());
        return intent;
    }
}
