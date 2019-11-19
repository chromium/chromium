// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.util.IntentUtils;

/**
 * Activity for displaying the browsing history manager.
 */
public class HistoryActivity extends SnackbarActivity {
    private HistoryManager mHistoryManager;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        boolean isIncognito = IntentUtils.safeGetBooleanExtra(
                getIntent(), IntentHandler.EXTRA_INCOGNITO_MODE, false);
        mHistoryManager = new HistoryManager(this, true, getSnackbarManager(), isIncognito);
        setContentView(mHistoryManager.getView());
    }

    @Override
    protected void onDestroy() {
        mHistoryManager.onDestroyed();
        mHistoryManager = null;
        super.onDestroy();
    }

    @VisibleForTesting
    HistoryManager getHistoryManagerForTests() {
        return mHistoryManager;
    }

    @Override
    public void onBackPressed() {
        if (!mHistoryManager.onBackPressed()) super.onBackPressed();
    }
}
