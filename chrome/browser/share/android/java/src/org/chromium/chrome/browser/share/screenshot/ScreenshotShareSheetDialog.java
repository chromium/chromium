// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.app.Dialog;
import android.content.Context;
import android.graphics.Bitmap;
import android.os.Bundle;

import androidx.fragment.app.DialogFragment;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
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
    private Callback<Runnable> mInstallCallback;

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
            Callback<Runnable> installCallback) {
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
        return null;
    }
}
