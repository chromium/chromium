// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge.OnClearBrowsingDataListener;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;

/**
 * An implementation of the {@link QuickDeleteDelegate} to handle quick delete operations
 * for Chrome.
 */
public class QuickDeleteDelegateImpl implements QuickDeleteDelegate {
    @Override
    public void performQuickDelete(@NonNull Runnable onDeleteFinished) {
        BrowsingDataBridge.getInstance().clearBrowsingData(
                new OnClearBrowsingDataListener() {
                    @Override
                    public void onBrowsingDataCleared() {
                        onDeleteFinished.run();
                    }
                },
                new int[] {
                        BrowsingDataType.HISTORY, BrowsingDataType.COOKIES, BrowsingDataType.CACHE},
                TimePeriod.LAST_15_MINUTES);
    }
}
