// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.view.View;

import org.chromium.base.CallbackUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Manager class for the tab bottom sheet. */
@NullMarked
public class TabBottomSheetManager implements Destroyable {
    private static final int INVALID_REQUEST_ID = -1;
    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver;
    private @SheetState int mSheetState = SheetState.HIDDEN;
    private int mRequestId = INVALID_REQUEST_ID;
    private @Nullable TabBottomSheetToolbar mToolbar;
    private @Nullable TabBottomSheetWebUi mWebUi;
    private @Nullable TabBottomSheetFusebox mFusebox;
    private @Nullable TabBottomSheetCoordinator mTabBottomSheetCoordinator;

    /**
     * Constructor.
     *
     * @param activity The current {@link Activity} instance.
     * @param profileSupplier A supplier for the current {@link Profile}.
     * @param windowAndroid The {@link WindowAndroid} for managing window-level operations.
     * @param lifecycleDispatcher The {@link ActivityLifecycleDispatcher} for managing activity
     *     lifecycle.
     * @param snackbarManager The {@link SnackbarManager} for showing snackbars.
     * @param bottomSheetController The {@link BottomSheetController} used to show the bottom sheet.
     */
    public TabBottomSheetManager(
            Activity activity,
            MonotonicObservableSupplier<Profile> profileSupplier,
            WindowAndroid windowAndroid,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            SnackbarManager snackbarManager,
            BottomSheetController bottomSheetController) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mBottomSheetController = bottomSheetController;
        mToolbar = new TabBottomSheetSimpleToolbar(activity);
        mWebUi = new TabBottomSheetWebUi(activity, windowAndroid);
        if (TabBottomSheetUtils.shouldShowFusebox()) {
            mFusebox =
                    new TabBottomSheetFusebox(
                            activity,
                            profileSupplier,
                            windowAndroid,
                            lifecycleDispatcher,
                            CallbackUtils.emptyCallback(),
                            snackbarManager);
        }

        mBottomSheetObserver = buildBottomSheetObserver();
        TabBottomSheetUtils.attachManagerToWindow(windowAndroid, this);
    }

    /**
     * Attempts to show the Tab BottomSheet. The boolean params are temporary, they will be moved
     * into enums later to allow more flexibility.
     *
     * @param requestId The request id for the bottom sheet.
     * @param shouldShowToolbar Whether to show the toolbar.
     * @param shouldShowFusebox Whether to show the fusebox.
     * @return Whether the bottom sheet was shown.
     */
    boolean tryToShowBottomSheet(
            int requestId, boolean shouldShowToolbar, boolean shouldShowFusebox) {
        if (TabBottomSheetUtils.isTabBottomSheetEnabled()) {
            assert mWebUi != null : "WebUi should not be null";
            if (mTabBottomSheetCoordinator == null) {
                mTabBottomSheetCoordinator =
                        new TabBottomSheetCoordinator(mActivity, mBottomSheetController);
            }

            // Handle requests.
            // Another sheet is showing.
            if (mSheetState != SheetState.HIDDEN) return false;
            mRequestId = requestId;

            View toolbarView =
                    mToolbar != null && shouldShowToolbar ? mToolbar.getToolbarView() : null;
            View webUiView = mWebUi.getWebUiView();
            View fuseboxView =
                    mFusebox != null && shouldShowFusebox ? mFusebox.getFuseboxView() : null;
            if (mTabBottomSheetCoordinator.tryToShowBottomSheet(
                    toolbarView, webUiView, fuseboxView)) {
                // Successfully showed bottom sheet.
                mBottomSheetController.addObserver(mBottomSheetObserver);
                return true;
            }
        }
        // Failed to show bottom sheet.
        return false;
    }

    void tryToCloseBottomSheet(int requestId) {
        if (mTabBottomSheetCoordinator != null && mRequestId == requestId) {
            mTabBottomSheetCoordinator.closeBottomSheet();
        }
    }

    boolean isSheetShowing(int requestId) {
        return mTabBottomSheetCoordinator != null
                && mRequestId == requestId
                && mSheetState != SheetState.HIDDEN;
    }

    boolean setWebContents(@Nullable WebContents webContents) {
        if (mWebUi != null) {
            mWebUi.setWebContents(webContents);
            return true;
        }
        return false;
    }

    @Nullable WebContents getWebContents(int requestId) {
        if (mWebUi != null && mRequestId == requestId) {
            return mWebUi.getWebContents();
        }
        return null;
    }

    // Observer methods.
    private BottomSheetObserver buildBottomSheetObserver() {
        return new EmptyBottomSheetObserver() {
            @Override
            public void onSheetOpened(@StateChangeReason int reason) {
                if (mFusebox != null) {
                    mFusebox.onBottomSheetShown();
                }
            }

            @Override
            public void onSheetStateChanged(@SheetState int state, @StateChangeReason int reason) {
                mSheetState = state;
            }

            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                mBottomSheetController.removeObserver(mBottomSheetObserver);
                assumeNonNull(mTabBottomSheetCoordinator).destroy();
            }
        };
    }

    @Override
    public void destroy() {
        if (mTabBottomSheetCoordinator != null) {
            mTabBottomSheetCoordinator.destroy();
            mTabBottomSheetCoordinator = null;
        }
        if (mToolbar != null) {
            mToolbar = null;
        }
        if (mWebUi != null) {
            mWebUi.destroy();
            mWebUi = null;
        }
        if (mFusebox != null) {
            mFusebox.destroy();
            mFusebox = null;
        }
        TabBottomSheetUtils.detachManagerFromWindow(mWindowAndroid);
    }

    /* Testing methods */
    public @Nullable TabBottomSheetCoordinator getTabBottomSheetCoordinatorForTesting() {
        return mTabBottomSheetCoordinator;
    }
}
