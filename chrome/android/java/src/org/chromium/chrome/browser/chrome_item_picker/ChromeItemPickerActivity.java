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
import org.chromium.chrome.browser.page_content_annotations.PageContentExtractionService;
import org.chromium.chrome.browser.page_content_annotations.PageContentExtractionServiceFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;

/** An activity that serves as an entry point for selecting Chrome items, like tabs. */
@NullMarked
public class ChromeItemPickerActivity extends SnackbarActivity {
    private static final String TAG = "ChromeItemPicker";

    private int mWindowId;
    private @Nullable TabItemPickerCoordinator mItemPickerCoordinator;
    private @Nullable PageContentExtractionService mPageContentExtractionService;

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

        mItemPickerCoordinator =
                new TabItemPickerCoordinator(
                        getProfileSupplier(),
                        mWindowId,
                        this,
                        this.getSnackbarManager(),
                        rootView,
                        containerView);

        mItemPickerCoordinator.showTabItemPicker(this::handleModelFailure);
    }

    @Override
    protected void onProfileAvailable(Profile profile) {
        super.onProfileAvailable(profile);
        mPageContentExtractionService = PageContentExtractionServiceFactory.getForProfile(profile);
        mPageContentExtractionService.getAllCachedTabIds(
                (tabIds) -> {
                    // TODO(haileywang): Display the list of tabs.
                });
    }

    @Override
    protected void onDestroy() {
        if (mItemPickerCoordinator != null) {
            mItemPickerCoordinator.destroy();
        }

        super.onDestroy();
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
