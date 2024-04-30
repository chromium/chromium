// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.chrome.browser.history.HistoryManagerToolbar.InfoHeaderPref;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/**
 * {@link InfoHeaderPref} for app-specific history. History UI starts with the text visible only for
 * the first time user opens the history page. From the next time, the UI opens with the info text
 * hidden, visible by the info toggle button.
 */
class AppHistoryInfoHeaderPref implements InfoHeaderPref {
    @Override
    public boolean isVisible() {
        boolean infoSeen =
                ChromeSharedPreferences.getInstance()
                        .readBoolean(ChromePreferenceKeys.HISTORY_APP_SPECIFIC_INFO_SEEN, false);
        if (!infoSeen) {
            ChromeSharedPreferences.getInstance()
                    .writeBoolean(ChromePreferenceKeys.HISTORY_APP_SPECIFIC_INFO_SEEN, true);
        }
        return !infoSeen; // Should show the text if not already seen by users.
    }
}
