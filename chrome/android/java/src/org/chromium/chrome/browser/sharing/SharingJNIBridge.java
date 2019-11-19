// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing;

import android.content.Context;
import android.telephony.TelephonyManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;

/**
 * JNI bridge for SharingService.
 */
public class SharingJNIBridge {
    private static final String TAG = "SharingJNIBridge";

    // Returns if device supports telephony capability.
    @CalledByNative
    public static boolean isTelephonySupported() {
        Context context = ContextUtils.getApplicationContext();
        TelephonyManager tm =
                (TelephonyManager) context.getSystemService(Context.TELEPHONY_SERVICE);
        return (tm.getPhoneType() != TelephonyManager.PHONE_TYPE_NONE);
    }
}
