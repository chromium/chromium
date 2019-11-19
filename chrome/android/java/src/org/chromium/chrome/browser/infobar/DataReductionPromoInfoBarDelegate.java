// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.infobar;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.datareduction.DataReductionProxyUma;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.widget.Toast;

/**
 * Provides JNI methods for DataReductionPromoInfoBar.
 */
public class DataReductionPromoInfoBarDelegate {
    /**
     * Launches the {@link InfoBar}.
     *
     * @param webContents The {@link WebContents} in which to launch the {@link InfoBar}.
     */
    static void launch(WebContents webContents) {
        DataReductionPromoInfoBarDelegateJni.get().launch(webContents);
    }

    private DataReductionPromoInfoBarDelegate() {
    }

    /**
     * Creates and begins the process for showing a DataReductionProxyInfoBarDelegate.
     */
    @CalledByNative
    private static InfoBar showPromoInfoBar() {
        return new DataReductionPromoInfoBar();
    }

    /**
     * Enables the data reduction proxy, records uma, and shows a confirmation toast.
     *
     * @param isPrimaryButton Whether the primary infobar button was clicked.
     * @param context An Android context.
     */
    @CalledByNative
    private static void accept() {
        Context context = ContextUtils.getApplicationContext();
        DataReductionProxyUma
                .dataReductionProxyUIAction(DataReductionProxyUma.ACTION_INFOBAR_ENABLED);
        DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(
                context, true);
        Toast.makeText(context, context.getString(R.string.data_reduction_enabled_toast_lite_mode),
                     Toast.LENGTH_LONG)
                .show();
    }

    /**
     * When the infobar closes and the data reduction proxy is not enabled, record that the infobar
     * was dismissed.
     */
    @CalledByNative
    private static void onNativeDestroyed() {
        if (DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()) return;
        DataReductionProxyUma
                .dataReductionProxyUIAction(DataReductionProxyUma.ACTION_INFOBAR_DISMISSED);
    }

    @NativeMethods
    interface Natives {
        void launch(WebContents webContents);
    }
}
