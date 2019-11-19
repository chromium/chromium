// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import android.telephony.CellInfo;
import android.telephony.TelephonyManager;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.compat.ApiHelperForQ;

import java.util.List;

/**
 * Wrapper class to delegate requests for {@link CellInfo} data to {@link TelephonyManager}.
 *
 * TODO(crbug.com/954620): Replace this class once P builds are no longer supported.
 */
class CellInfoDelegate {
    static void requestCellInfoUpdate(
            TelephonyManager telephonyManager, Callback<List<CellInfo>> callback) {
        if (BuildInfo.isAtLeastQ()) {
            ApiHelperForQ.requestCellInfoUpdate(telephonyManager, callback);
            return;
        }
        callback.onResult(telephonyManager.getAllCellInfo());
    }
}
