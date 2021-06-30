// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.chrome.browser.history.HistoryDeletionBridge;
import org.chromium.chrome.browser.history.HistoryDeletionInfo;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;

/** History deletion observer that clears autofill-assistant flags when necessary. */
public class AutofillAssistantHistoryDeletionObserver implements HistoryDeletionBridge.Observer {
    @Override
    public void onURLsDeleted(HistoryDeletionInfo historyDeletionInfo) {
        if (!historyDeletionInfo.isTimeRangeForAllTime()) {
            return;
        }
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.AUTOFILL_ASSISTANT_FIRST_TIME_LITE_SCRIPT_USER);
    }
}
