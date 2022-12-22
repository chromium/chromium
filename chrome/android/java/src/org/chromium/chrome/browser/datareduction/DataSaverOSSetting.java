// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.datareduction;

import android.content.Context;
import android.net.ConnectivityManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.compat.ApiHelperForN;

/**
 * Wrapper for the datareduction::DataSaverOSSetting.
 */
@JNINamespace("datareduction::android")
public class DataSaverOSSetting {
    @CalledByNative
    public static boolean isDataSaverEnabled() {
        Context context = ContextUtils.getApplicationContext();
        ConnectivityManager connMgr =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        if (connMgr.isActiveNetworkMetered()) {
            return ApiHelperForN.getRestrictBackgroundStatus(connMgr)
                    == ConnectivityManager.RESTRICT_BACKGROUND_STATUS_ENABLED;
        }
        return false;
    }
}
