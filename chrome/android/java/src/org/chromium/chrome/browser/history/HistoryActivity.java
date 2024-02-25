// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.back_press.SecondaryActivityBackPressUma.SecondaryActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;

/** Activity for displaying the browsing history manager. */
public class HistoryActivity extends SnackbarActivity {
    private HistoryManager mHistoryManager;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        boolean isIncognito =
                IntentUtils.safeGetBooleanExtra(
                        getIntent(), IntentHandler.EXTRA_INCOGNITO_MODE, false);
        boolean isFromCustomTabActivity =
                getIntent().getBooleanExtra(Intent.EXTRA_RETURN_RESULT, false);
        String clientPackageName =
                IntentUtils.safeGetStringExtra(getIntent(), Intent.EXTRA_PACKAGE_NAME);
        Profile profile = getProfileProvider().getOriginalProfile();
        mHistoryManager =
                new HistoryManager(
                        this,
                        true,
                        getSnackbarManager(),
                        ProfileProvider.getOrCreateProfile(getProfileProvider(), isIncognito),
                        /* Supplier<Tab>= */ null,
                        new BrowsingHistoryBridge(profile),
                        clientPackageName,
                        !isFromCustomTabActivity);
        setContentView(mHistoryManager.getView());
        BackPressHelper.create(
                this, getOnBackPressedDispatcher(), mHistoryManager, SecondaryActivity.HISTORY);
    }

    @Override
    protected void onDestroy() {
        mHistoryManager.onDestroyed();
        mHistoryManager = null;
        super.onDestroy();
    }

    HistoryManager getHistoryManagerForTests() {
        return mHistoryManager;
    }
}
