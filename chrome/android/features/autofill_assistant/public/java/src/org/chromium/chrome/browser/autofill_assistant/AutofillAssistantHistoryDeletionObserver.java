// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.chrome.browser.history.HistoryDeletionBridge;
import org.chromium.chrome.browser.history.HistoryDeletionInfo;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.user_prefs.UserPrefs;

/** History deletion observer that clears autofill-assistant flags when necessary. */
public class AutofillAssistantHistoryDeletionObserver implements HistoryDeletionBridge.Observer {
    @Override
    public void onURLsDeleted(HistoryDeletionInfo historyDeletionInfo) {
        if (!historyDeletionInfo.isTimeRangeForAllTime()) {
            return;
        }
        UserPrefs.get(Profile.getLastUsedRegularProfile())
                .clearPref(Pref.AUTOFILL_ASSISTANT_TRIGGER_SCRIPTS_IS_FIRST_TIME_USER);
    }
}
