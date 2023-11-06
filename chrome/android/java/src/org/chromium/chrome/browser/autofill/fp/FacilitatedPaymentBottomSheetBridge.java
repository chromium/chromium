// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.fp;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Bridge class providing an entry point for autofill client to trigger the
 * facilitated payment bottom sheet.
 */
@JNINamespace("autofill")
public class FacilitatedPaymentBottomSheetBridge {
    private Context mContext;
    private BottomSheetController mBottomSheetController;

    @CalledByNative
    @VisibleForTesting
    /* package */ FacilitatedPaymentBottomSheetBridge() {}

    /**
     * Requests to show the bottom sheet.
     *
     * The bottom sheet may not be shown in some cases.
     * {@see BottomSheetController#requestShowContent}
     *
     * @return True if shown. False if it was suppressed. Content is suppressed if higher
     *         priority content is in the sheet, the sheet is expanded beyond the peeking state,
     *         or the browser is in a mode that does not support showing the sheet.
     */
    @CalledByNative
    public boolean requestShowContent(WebContents webContents) {
        if (webContents == null || webContents.isDestroyed()) return false;

        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return false;

        mBottomSheetController = BottomSheetControllerProvider.from(window);

        mContext = window.getContext().get();
        return (mContext == null)
                ? false
                : mBottomSheetController.requestShowContent(
                        new FacilitatedPaymentBottomSheetContent(mContext), /*animate=*/true);
    }
}
