// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import org.chromium.chrome.browser.history.HistoryManagerToolbar.InfoHeaderPref;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

class BrowserHistoryInfoHeaderPref implements InfoHeaderPref {
    @Override
    public boolean isVisible() {
        return ChromeSharedPreferences.getInstance()
                .readBoolean(ChromePreferenceKeys.HISTORY_SHOW_HISTORY_INFO, true);
    }

    @Override
    public void setVisible(boolean show) {
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.HISTORY_SHOW_HISTORY_INFO, show);
    }
}
