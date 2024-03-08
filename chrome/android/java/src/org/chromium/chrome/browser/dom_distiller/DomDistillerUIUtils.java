// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.app.Activity;

import androidx.appcompat.app.AlertDialog;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

/** Java implementation of dom_distiller::android::DistillerUIHandleAndroid. */
@JNINamespace("dom_distiller::android")
public final class DomDistillerUIUtils {
    /**
     * A static method for native code to call to open the distiller UI settings.
     * @param webContents The WebContents containing the distilled content.
     */
    @CalledByNative
    public static void openSettings(WebContents webContents) {
        if (webContents == null) return;

        Activity activity = ActivityUtils.getActivityFromWebContents(webContents);
        if (activity == null) return;

        RecordUserAction.record("DomDistiller_DistilledPagePrefsOpened");
        AlertDialog.Builder builder =
                new AlertDialog.Builder(activity, R.style.ThemeOverlay_BrowserUI_AlertDialog);
        builder.setView(
                DistilledPagePrefsView.create(
                        activity,
                        DomDistillerServiceFactory.getForProfile(
                                        Profile.fromWebContents(webContents))
                                .getDistilledPagePrefs()));
        builder.show();
    }

    private DomDistillerUIUtils() {}
}
