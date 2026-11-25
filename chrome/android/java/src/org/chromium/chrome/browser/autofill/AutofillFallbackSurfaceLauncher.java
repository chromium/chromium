// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.WindowAndroid;

/** A helper class to open web pages from the keyboard accessory manual filling UI. */
@JNINamespace("autofill")
@NullMarked
public class AutofillFallbackSurfaceLauncher {

    @CalledByNative
    public static void openGoogleWalletPassesPage(WindowAndroid window) {
        Context context = window.getActivity().get();

        if (context == null) {
            return;
        }
        openGoogleWalletPassesPage(context);
    }

    public static void openGoogleWalletPassesPage(Context context) {
        GoogleWalletLauncher.openGoogleWallet(context, context.getPackageManager());
    }

    @CalledByNative
    public static void openGoogleWalletPrivatePassHelpCenterPageInCct(WindowAndroid window) {
        Context context = window.getActivity().get();

        if (context == null) {
            return;
        }
        GoogleWalletLauncher.openGoogleWalletPrivatePassHelpCenterPage(context);
    }

    @CalledByNative
    public static void openGooglePrivacyPolicyPageInCct(WindowAndroid window) {
        Context context = window.getActivity().get();

        if (context == null) {
            return;
        }
        GoogleWalletLauncher.openGooglePrivacyPolicyPage(context);
    }

    private AutofillFallbackSurfaceLauncher() {}
}
