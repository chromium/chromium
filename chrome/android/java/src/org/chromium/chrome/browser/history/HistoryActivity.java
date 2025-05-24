// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.content.Intent;
import android.view.LayoutInflater;
import android.view.ViewGroup;

import org.chromium.base.CallbackUtils;
import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.ui.KeyboardVisibilityDelegate;

/** Activity for displaying the browsing history manager. */
public class HistoryActivity extends SnackbarActivity {
    private HistoryManager mHistoryManager;
    private ManagedBottomSheetController mBottomSheetController;

    @Override
    protected void onProfileAvailable(Profile profile) {
        super.onProfileAvailable(profile);

        boolean appSpecificHistory =
                getIntent().getBooleanExtra(IntentHandler.EXTRA_APP_SPECIFIC_HISTORY, false);
        // For now, we only hide the clear data button for app specific history.
        boolean shouldShowClearData = !appSpecificHistory;
        String clientPackageName =
                IntentUtils.safeGetStringExtra(getIntent(), Intent.EXTRA_PACKAGE_NAME);
        HistoryUmaRecorder historyUmaRecorder =
                appSpecificHistory ? new AppHistoryUmaRecorder() : new HistoryUmaRecorder();
        boolean showAppFilter = !appSpecificHistory && !profile.isOffTheRecord();
        mHistoryManager =
                new HistoryManager(
                        this,
                        true,
                        getSnackbarManager(),
                        profile,
                        () -> mBottomSheetController,
                        /* Supplier<Tab>= */ null,
                        new BrowsingHistoryBridge(profile.getOriginalProfile()),
                        historyUmaRecorder,
                        clientPackageName,
                        shouldShowClearData,
                        appSpecificHistory,
                        showAppFilter,
                        /* openHistoryItemCallback= */ null);
        ViewGroup contentView = mHistoryManager.getView();
        setContentView(contentView);
        if (showAppFilter) createBottomSheetController(contentView);
        BackPressHelper.create(this, getOnBackPressedDispatcher(), mHistoryManager);
    }

    private void createBottomSheetController(ViewGroup contentView) {
        ViewGroup sheetContainer =
                (ViewGroup)
                        LayoutInflater.from(this).inflate(R.layout.bottom_sheet_container, null);
        ScrimManager scrimManager = new ScrimManager(this, contentView);
        mBottomSheetController =
                BottomSheetControllerFactory.createBottomSheetController(
                        () -> scrimManager,
                        CallbackUtils.emptyCallback(),
                        getWindow(),
                        KeyboardVisibilityDelegate.getInstance(),
                        () -> sheetContainer,
                        () -> 0,
                        /* desktopWindowStateManager= */ null);

        // HistoryActivity needs its own container for bottom sheet. Add it as a child of the
        // layout enclosing the history list layout so they'll be siblings. HistoryPage doesn't
        // need this since it may share the one from tabbed browser activity.
        contentView.addView(sheetContainer);
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
