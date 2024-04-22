// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Intent;
import android.os.Bundle;
import android.view.ViewGroup;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.back_press.SecondaryActivityBackPressUma.SecondaryActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;

/** Activity for displaying the browsing history manager. */
public class HistoryActivity extends SnackbarActivity {
    private HistoryManager mHistoryManager;
    private ManagedBottomSheetController mBottomSheetController;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        boolean isIncognito =
                IntentUtils.safeGetBooleanExtra(
                        getIntent(), IntentHandler.EXTRA_INCOGNITO_MODE, false);
        boolean appSpecificHistory =
                getIntent().getBooleanExtra(IntentHandler.EXTRA_APP_SPECIFIC_HISTORY, false);
        // For now, we only hide the clear data button for app specific history.
        boolean shouldShowClearData = !appSpecificHistory;
        String clientPackageName =
                IntentUtils.safeGetStringExtra(getIntent(), Intent.EXTRA_PACKAGE_NAME);
        Profile profile = getProfileProvider().getOriginalProfile();
        HistoryUmaRecorder historyUmaRecorder =
                appSpecificHistory ? new AppHistoryUmaRecorder() : new HistoryUmaRecorder();
        boolean showAppFilter = !appSpecificHistory;
        mHistoryManager =
                new HistoryManager(
                        this,
                        true,
                        getSnackbarManager(),
                        ProfileProvider.getOrCreateProfile(getProfileProvider(), isIncognito),
                        () -> mBottomSheetController,
                        /* Supplier<Tab>= */ null,
                        new BrowsingHistoryBridge(profile),
                        historyUmaRecorder,
                        clientPackageName,
                        shouldShowClearData,
                        appSpecificHistory,
                        showAppFilter);
        ViewGroup contentView = mHistoryManager.getView();
        setContentView(contentView);
        if (showAppFilter) createBottomSheetController(contentView);
        BackPressHelper.create(
                this, getOnBackPressedDispatcher(), mHistoryManager, SecondaryActivity.HISTORY);
    }

    private void createBottomSheetController(ViewGroup contentView) {
        // TODO: Create bottomsheet controller for HistoryActivity.
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
