// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.app.Dialog;
import android.graphics.Bitmap;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.components.browser_ui.widget.FullscreenAlertDialog;
import org.chromium.ui.base.WindowAndroid;

/** ScreenshotShareSheetDialog is the main view for sharing non edited screenshots. */
public class ScreenshotShareSheetDialog extends DialogFragment {
    private Bitmap mScreenshot;
    private WindowAndroid mWindowAndroid;
    private String mShareUrl;
    private ChromeOptionShareCallback mChromeOptionShareCallback;

    /** The ScreenshotShareSheetDialog constructor. */
    public ScreenshotShareSheetDialog() {}

    /**
     * Initialize the dialog outside of the constructor as fragments require default constructor.
     *
     * @param screenshot The screenshot image to show.
     * @param windowAndroid The associated {@link WindowAndroid}.
     * @param shareUrl The URL associated with the screenshot.
     * @param chromeOptionShareCallback the callback to trigger on share.
     */
    public void init(
            Bitmap screenshot,
            WindowAndroid windowAndroid,
            String shareUrl,
            ChromeOptionShareCallback chromeOptionShareCallback) {
        mScreenshot = screenshot;
        mWindowAndroid = windowAndroid;
        mShareUrl = shareUrl;
        mChromeOptionShareCallback = chromeOptionShareCallback;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Do not recreate this dialog when activity restarts and the previous activity is gone.
        if (mWindowAndroid == null
                || mWindowAndroid.getActivity().get() == null
                || mWindowAndroid.getActivity().get().isDestroyed()
                || mWindowAndroid.getActivity().get().isFinishing()) {
            dismiss();
        }
    }

    @NonNull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        AlertDialog.Builder builder = new FullscreenAlertDialog.Builder(getActivity());
        ScreenshotShareSheetView screenshotShareSheetView =
                (ScreenshotShareSheetView)
                        getActivity()
                                .getLayoutInflater()
                                .inflate(R.layout.screenshot_share_sheet, null);
        builder.setView(screenshotShareSheetView);

        new ScreenshotShareSheetCoordinator(
                getActivity(),
                mScreenshot,
                this::dismissAllowingStateLoss,
                screenshotShareSheetView,
                mWindowAndroid,
                mShareUrl,
                mChromeOptionShareCallback);
        return builder.create();
    }
}
