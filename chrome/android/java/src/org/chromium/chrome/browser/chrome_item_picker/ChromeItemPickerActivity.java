// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.chrome_item_picker;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.view.ViewGroup;

import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMediator;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorItemSelectionId;

import java.util.Set;

/** An activity that serves as an entry point for selecting Chrome items, like tabs. */
@NullMarked
public class ChromeItemPickerActivity extends SnackbarActivity {
    private static final String TAG = "ChromeItemPicker";
    private int mWindowId;
    private @Nullable TabItemPickerCoordinator mItemPickerCoordinator;

    @Override
    protected void onCreateInternal(@Nullable Bundle savedInstanceState) {
        super.onCreateInternal(savedInstanceState);
        setContentView(R.layout.chrome_item_picker_activity);

        ViewGroup containerView = findViewById(R.id.chrome_item_picker_container);
        ViewGroup rootView = containerView;

        mWindowId =
                IntentUtils.safeGetIntExtra(
                        getIntent(),
                        IntentHandler.EXTRA_WINDOW_ID,
                        TabWindowManager.INVALID_WINDOW_ID);
        if (mWindowId == TabWindowManager.INVALID_WINDOW_ID) {
            mWindowId = MultiWindowUtils.getLastAccessedWindowId();
        }
        if (mWindowId == TabWindowManager.INVALID_WINDOW_ID) {
            handleFailureToShowPicker("Could not determine a valid target browser window ID.");
            return;
        }

        Long[] preselectedIds =
                IntentUtils.safeGetSerializableExtra(
                        getIntent(), FuseboxMediator.EXTRA_PRESELECTED_TAB_IDS);
        if (preselectedIds == null) {
            // TODO(bbetini): Use a helper method to create an empty list when preselectedIds is
            // null.
            preselectedIds = new Long[0];
        }

        mItemPickerCoordinator =
                new TabItemPickerCoordinator(
                        getProfileSupplier(),
                        mWindowId,
                        this,
                        this.getSnackbarManager(),
                        rootView,
                        containerView,
                        preselectedIds);

        mItemPickerCoordinator.showTabItemPicker(this::handleModelFailure);
    }

    @Override
    protected void onDestroy() {
        if (mItemPickerCoordinator != null) {
            mItemPickerCoordinator.destroy();
        }

        super.onDestroy();
    }

    // TODO(bbetini): Make method private when it is set to be the callback of
    // TabItemPickerCoordinator.showTabItemPicker().
    public void finishWithSelectedItems(Set<TabListEditorItemSelectionId> selectedItems) {
        long[] tabIds = new long[selectedItems.size()];
        int i = 0;

        for (TabListEditorItemSelectionId selectionId : selectedItems) {
            tabIds[i++] = selectionId.getTabId();
        }

        final Intent resultIntent = new Intent();

        resultIntent.putExtra(FuseboxMediator.EXTRA_ATTACHMENT_TAB_IDS, tabIds);
        setResult(Activity.RESULT_OK, resultIntent);
        finish();
    }

    public void finishWithCancel() {
        setResult(Activity.RESULT_CANCELED, new Intent());
        finish();
    }

    private void handleModelFailure(@Nullable TabModelSelector tabModelSelector) {
        if (tabModelSelector == null) {
            handleFailureToShowPicker("Failed to launch activity.");
            return;
        }
    }

    private void handleFailureToShowPicker(String error) {
        Log.e(TAG, error);
        final Intent resultIntent = new Intent();
        resultIntent.putExtra(IntentHandler.EXTRA_ITEM_PICKER_ERROR, error);
        setResult(Activity.RESULT_CANCELED, resultIntent);
        finish();
    }
}
