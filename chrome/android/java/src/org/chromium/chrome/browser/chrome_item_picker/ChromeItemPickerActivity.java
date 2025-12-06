// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.chrome_item_picker;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;

import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.incognito.IncognitoWindowNightModeStateProvider;
import org.chromium.chrome.browser.incognito_window.PreAttachIntentObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMediator;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorItemSelectionId;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;

import java.util.ArrayList;
import java.util.List;

/** An activity that serves as an entry point for selecting Chrome items, like tabs. */
@NullMarked
public class ChromeItemPickerActivity extends SnackbarActivity implements PreAttachIntentObserver {
    private static final String TAG = "ChromeItemPicker";
    private int mWindowId;
    private @Nullable TabItemPickerCoordinator mItemPickerCoordinator;
    private boolean mIsIncognito;
    private @Nullable IncognitoWindowNightModeStateProvider mIncognitoWindowNightModeStateProvider;

    @Override
    protected void onCreateInternal(@Nullable Bundle savedInstanceState) {
        super.onCreateInternal(savedInstanceState);
        setContentView(R.layout.chrome_item_picker_activity);
        initializeSystemBarColors(
                assumeNonNull(getEdgeToEdgeManager()).getEdgeToEdgeSystemBarColorHelper());

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

        ArrayList<Integer> preselectedIds =
                getIntent().getIntegerArrayListExtra(FuseboxMediator.EXTRA_PRESELECTED_TAB_IDS);
        if (preselectedIds == null) {
            // TODO(bbetini): Use a helper method to create an empty list when preselectedIds is
            // null.
            preselectedIds = new ArrayList<Integer>();
        }

        int allowedSelectionCount =
                IntentUtils.safeGetIntExtra(
                        getIntent(), FuseboxMediator.EXTRA_ALLOWED_SELECTION_COUNT, 0);

        mItemPickerCoordinator =
                new TabItemPickerCoordinator(
                        getProfileSupplier(),
                        mWindowId,
                        this,
                        this.getSnackbarManager(),
                        rootView,
                        containerView,
                        preselectedIds,
                        allowedSelectionCount);

        mItemPickerCoordinator.showTabItemPicker(this::handleModelFailure);
    }

    @Override
    protected void onDestroy() {
        if (mItemPickerCoordinator != null) {
            mItemPickerCoordinator.destroy();
        }

        super.onDestroy();
    }

    @Override
    public void onPreAttachIntentAvailable(Intent intent) {
        setIsIncognito(intent);
    }

    @Override
    protected void initializeSystemBarColors(
            EdgeToEdgeSystemBarColorHelper edgeToEdgeSystemBarColorHelper) {
        @ColorInt
        int backgroundColor =
                ChromeColors.getDefaultThemeColor(this, /* isIncognito= */ mIsIncognito);
        edgeToEdgeSystemBarColorHelper.setStatusBarColor(backgroundColor);
        edgeToEdgeSystemBarColorHelper.setNavigationBarColor(backgroundColor);
    }

    @Override
    protected void applyThemeOverlays() {
        super.applyThemeOverlays();
        if (mIsIncognito) {
            applySingleThemeOverlay(R.style.ThemeOverlay_BrowserUI_TabbedMode_Incognito);
        }
    }

    @Override
    protected void attachBaseContext(Context newBase) {
        if (getIntent() != null) {
            setIsIncognito(getIntent());
        }

        super.attachBaseContext(newBase);
    }

    @Override
    protected void initializeNightModeStateProvider() {
        if (mIncognitoWindowNightModeStateProvider != null) {
            mIncognitoWindowNightModeStateProvider.initialize(getDelegate());
        } else {
            super.initializeNightModeStateProvider();
        }
    }

    @Override
    protected NightModeStateProvider createNightModeStateProvider() {
        if (mIsIncognito) {
            mIncognitoWindowNightModeStateProvider = new IncognitoWindowNightModeStateProvider();
            return mIncognitoWindowNightModeStateProvider;
        }
        return super.createNightModeStateProvider();
    }

    private void setIsIncognito(@Nullable Intent intent) {
        if (intent == null) return;

        mIsIncognito =
                IntentUtils.safeGetBooleanExtra(
                        intent,
                        FuseboxMediator.EXTRA_IS_INCOGNITO_BRANDED,
                        /* defaultValue= */ false);
    }

    // TODO(bbetini): Make method private when it is set to be the callback of
    // TabItemPickerCoordinator.showTabItemPicker().
    public void finishWithSelectedItems(List<TabListEditorItemSelectionId> selectedItems) {
        ArrayList<Integer> tabIds = new ArrayList<>();
        for (TabListEditorItemSelectionId selectionId : selectedItems) {
            tabIds.add(selectionId.getTabId());
        }

        final Intent resultIntent = new Intent();

        resultIntent.putIntegerArrayListExtra(FuseboxMediator.EXTRA_ATTACHMENT_TAB_IDS, tabIds);
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
