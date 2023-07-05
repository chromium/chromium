// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;

/**
 * An implementation of the {@link QuickDeleteDelegate} to handle quick delete operations
 * for Chrome.
 */
public class QuickDeleteDelegateImpl implements QuickDeleteDelegate {
    @Override
    public void performQuickDelete(@NonNull Runnable onDeleteFinished, @TimePeriod int timePeriod) {
        // Note: clang-format does a bad job formatting lambdas so we turn it off here.
        // clang-format off
        BrowsingDataBridge.getInstance().clearBrowsingData(
                onDeleteFinished::run,
                new int[] {
                        BrowsingDataType.HISTORY, BrowsingDataType.COOKIES, BrowsingDataType.CACHE
                },
                timePeriod);
        // clang-format on
    }
}
