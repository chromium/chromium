// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import static org.chromium.build.NullUtil.assertNonNull;

import android.app.Dialog;
import android.graphics.Bitmap;
import android.os.Bundle;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.components.browser_ui.widget.FullscreenAlertDialog;
import org.chromium.ui.base.WindowAndroid;

/** ScreenshotShareSheetDialog is the main view for sharing non edited screenshots. */
@NullMarked
public class ScreenshotShareSheetDialog extends DialogFragment {
    private Bitmap mScreenshot;
    private @Nullable WindowAndroid mWindowAndroid;
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
    @Initializer
    public void init(
            Bitmap screenshot,
            @Nullable WindowAndroid windowAndroid,
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

    @Override
    public Dialog onCreateDialog(@Nullable Bundle savedInstanceState) {
        AlertDialog.Builder builder =
                new FullscreenAlertDialog.Builder(
                        getActivity(), EdgeToEdgeUtils.isEdgeToEdgeEverywhereEnabled());
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
                assertNonNull(mWindowAndroid),
                mShareUrl,
                mChromeOptionShareCallback);
        return builder.create();
    }
}
