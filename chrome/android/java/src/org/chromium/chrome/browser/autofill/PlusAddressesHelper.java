// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.ui.base.WindowAndroid;

/** A helper class to open plus address management web page. */
@JNINamespace("plus_addresses")
public class PlusAddressesHelper {
    @CalledByNative
    public static void openManagePlusAddresses(
            WindowAndroid window, @JniType("std::string") String url) {
        Context context = window.getActivity().get();

        if (context == null) {
            return;
        }

        openManagePlusAddresses(context, url);
    }

    public static void openManagePlusAddresses(Context context, String url) {
        CustomTabActivity.showInfoPage(context, url);
    }

    public static String getPlusAddressManagementUrl() {
        return PlusAddressesHelperJni.get().getPlusAddressManagementUrl();
    }

    private PlusAddressesHelper() {}

    @NativeMethods
    interface Natives {
        String getPlusAddressManagementUrl();
    }
}
