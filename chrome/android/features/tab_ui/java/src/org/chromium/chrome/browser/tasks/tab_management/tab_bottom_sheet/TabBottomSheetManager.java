// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;

import org.chromium.base.CallbackUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** Manager class for the tab bottom sheet. */
@NullMarked
public class TabBottomSheetManager implements Destroyable {
    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final BottomSheetController mBottomSheetController;
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

        TabBottomSheetUtils.attachManagerToWindow(windowAndroid, this);
    }

    /**
     * Attempts to show the Tab BottomSheet. The boolean params are temporary, they will be moved
     * into enums later to allow more flexibility.
     *
     * @param shouldShowToolbar Whether to show the toolbar.
     * @param shouldShowFusebox Whether to show the fusebox.
     */
    void tryToShowBottomSheet(boolean shouldShowToolbar, boolean shouldShowFusebox) {
        if (TabBottomSheetUtils.isTabBottomSheetEnabled()) {
            if (mTabBottomSheetCoordinator == null) {
                mTabBottomSheetCoordinator =
                        new TabBottomSheetCoordinator(mActivity, mBottomSheetController);
            }
            mTabBottomSheetCoordinator.showBottomSheet(
                    mToolbar != null && shouldShowToolbar ? mToolbar.getToolbarView() : null,
                    assumeNonNull(mWebUi).getWebUiView(),
                    mFusebox != null && shouldShowFusebox ? mFusebox.getFuseboxView() : null,
                    this::onBottomSheetShowAttempted);
        }
    }

    void tryToCloseBottomSheet() {
        if (mTabBottomSheetCoordinator != null) {
            mTabBottomSheetCoordinator.closeBottomSheet();
        }
    }

    boolean isSheetShowing() {
        return mTabBottomSheetCoordinator != null && mTabBottomSheetCoordinator.isSheetShowing();
    }

    void setWebContents(WebContents webContents) {
        if (mWebUi != null) {
            mWebUi.setWebContents(webContents);
        }
    }

    @Nullable WebContents getWebContents() {
        return mWebUi != null ? mWebUi.getWebContents() : null;
    }

    void onBottomSheetShowAttempted(boolean didSucceed) {
        if (didSucceed && mFusebox != null) {
            mFusebox.onBottomSheetShown();
        }
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
