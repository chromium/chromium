// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.app.Dialog;
import android.content.Context;
import android.graphics.Bitmap;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.components.browser_ui.widget.FullscreenAlertDialog;
import org.chromium.ui.base.WindowAndroid;

/**
 * ScreenshotShareSheetDialog is the main view for sharing non edited screenshots.
 */
public class ScreenshotShareSheetDialog extends DialogFragment {
    private Context mContext;
    private Bitmap mScreenshot;
    private WindowAndroid mWindowAndroid;
    private String mShareUrl;
    private ChromeOptionShareCallback mChromeOptionShareCallback;
    private @Nullable Callback<Runnable> mInstallCallback;

    /**
     * The ScreenshotShareSheetDialog constructor.
     */
    public ScreenshotShareSheetDialog() {}

    /**
     * Initialize the dialog outside of the constructor as fragments require default constructor.
     * @param screenshot The screenshot image to show.
     * @param windowAndroid The associated {@link WindowAndroid}.
     * @param shareUrl The URL associated with the screenshot.
     * @param chromeOptionShareCallback the callback to trigger on share.
     * @param installCallback the callback to trigger on install.
     */
    public void init(Bitmap screenshot, WindowAndroid windowAndroid, String shareUrl,
            ChromeOptionShareCallback chromeOptionShareCallback,
            @Nullable Callback<Runnable> installCallback) {
        mScreenshot = screenshot;
        mInstallCallback = installCallback;
        mWindowAndroid = windowAndroid;
        mShareUrl = shareUrl;
        mChromeOptionShareCallback = chromeOptionShareCallback;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mContext = context;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        AlertDialog.Builder builder = new FullscreenAlertDialog.Builder(getActivity());
        ScreenshotShareSheetView screenshotShareSheetView =
                (ScreenshotShareSheetView) getActivity().getLayoutInflater().inflate(
                        R.layout.screenshot_share_sheet, null);
        builder.setView(screenshotShareSheetView);

        new ScreenshotShareSheetCoordinator(mContext, mScreenshot, this::dismissAllowingStateLoss,
                screenshotShareSheetView, mWindowAndroid, mShareUrl, mChromeOptionShareCallback,
                mInstallCallback);
        return builder.create();
    }
}
