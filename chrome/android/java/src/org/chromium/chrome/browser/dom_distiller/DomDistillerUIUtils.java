// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.app.Activity;

import androidx.appcompat.app.AlertDialog;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Java implementation of dom_distiller::android::DistillerUIHandleAndroid.
 */
@JNINamespace("dom_distiller::android")
public final class DomDistillerUIUtils {
    /**
     * A static method for native code to call to open the distiller UI settings.
     * @param webContents The WebContents containing the distilled content.
     */
    @CalledByNative
    public static void openSettings(WebContents webContents) {
        Activity activity = getActivityFromWebContents(webContents);

        if (webContents != null && activity != null) {
            RecordUserAction.record("DomDistiller_DistilledPagePrefsOpened");
            AlertDialog.Builder builder =
                    new AlertDialog.Builder(activity, R.style.Theme_Chromium_AlertDialog);
            builder.setView(DistilledPagePrefsView.create(activity));
            builder.show();
        }
    }

    /**
     * @param webContents The WebContents to get the Activity from.
     * @return The Activity associated with the WebContents.
     */
    private static Activity getActivityFromWebContents(WebContents webContents) {
        if (webContents == null) return null;

        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return null;

        return window.getActivity().get();
    }

    private DomDistillerUIUtils() {}
}
