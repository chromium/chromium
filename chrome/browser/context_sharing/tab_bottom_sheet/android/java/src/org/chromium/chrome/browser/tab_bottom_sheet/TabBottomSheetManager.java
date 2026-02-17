// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.app.Activity;
import android.view.View;

import org.chromium.base.CallbackUtils;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetFusebox.TabBottomSheetFuseboxConfig;
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

    // Interface for the native to communicate with the tab bottom sheet manager.
    interface NativeInterfaceDelegate {
        /** Inner class to hold the singleton instance. */
        static class LazyHolder {
            static final NativeInterfaceDelegate INSTANCE =
                    new NativeInterfaceDelegate() {
                        @Override
                        public void onBottomSheetClosed() {}

                        @Override
                        public long getRequestId() {
                            return 0;
                        }
                    };
        }

        static NativeInterfaceDelegate getInstance() {
            return LazyHolder.INSTANCE;
        }

        // Method called when the bottom sheet is closed.
        void onBottomSheetClosed();

        // Method called to get the request id.
        long getRequestId();
    }

    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver;

    private @Nullable TabBottomSheetToolbar mToolbar;
    private @Nullable TabBottomSheetWebUi mWebUi;
    private @Nullable TabBottomSheetFusebox mFusebox;
    private @Nullable TabBottomSheetCoordinator mTabBottomSheetCoordinator;
    private @Nullable NativeInterfaceDelegate mNativeInterfaceDelegate;

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
            @Nullable TabBottomSheetFuseboxConfig fuseboxConfig,
            NonNullObservableSupplier<Profile> profileSupplier,
            WindowAndroid windowAndroid,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            SnackbarManager snackbarManager,
            BottomSheetController bottomSheetController) {
        mActivity = activity;
        mWindowAndroid = windowAndroid;
        mBottomSheetController = bottomSheetController;
        mToolbar = new TabBottomSheetSimpleToolbar(activity);
        mWebUi = new TabBottomSheetWebUi(activity, windowAndroid);
        if (TabBottomSheetUtils.shouldShowFusebox() && fuseboxConfig != null) {
            mFusebox =
                    new TabBottomSheetFusebox(
                            activity,
                            fuseboxConfig,
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
     * @param nativeInterfaceDelegate The native interface delegate.
     * @param shouldShowToolbar Whether to show the toolbar.
     * @param shouldShowFusebox Whether to show the fusebox.
     * @return Whether the bottom sheet was shown.
     */
    boolean tryToShowBottomSheet(
            NativeInterfaceDelegate nativeInterfaceDelegate,
            boolean shouldShowToolbar,
            boolean shouldShowFusebox) {
        if (TabBottomSheetUtils.isTabBottomSheetEnabled()) {
            assert mWebUi != null : "WebUi should not be null";
            if (mTabBottomSheetCoordinator == null) {
                mTabBottomSheetCoordinator =
                        new TabBottomSheetCoordinator(mActivity, mBottomSheetController);
            }

            View toolbarView =
                    mToolbar != null && shouldShowToolbar ? mToolbar.getToolbarView() : null;
            View webUiView = mWebUi.getWebUiView();
            View fuseboxView =
                    mFusebox != null && shouldShowFusebox ? mFusebox.getFuseboxView() : null;
            if (mTabBottomSheetCoordinator.tryToShowBottomSheet(
                    toolbarView, webUiView, fuseboxView)) {
                // Successfully showed bottom sheet.
                mBottomSheetController.addObserver(mBottomSheetObserver);
                mNativeInterfaceDelegate = nativeInterfaceDelegate;
                return true;
            }
        }
        // Failed to show bottom sheet.
        return false;
    }

    void detachNativeInterfaceDelegate(NativeInterfaceDelegate delegate) {
        if (mNativeInterfaceDelegate == delegate) {
            mNativeInterfaceDelegate = null;
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

    boolean setWebContents(WebContents webContents) {
        if (mWebUi != null) {
            mWebUi.setWebContents(webContents);
            return true;
        }
        return false;
    }

    @Nullable WebContents getWebContents() {
        return mWebUi != null ? mWebUi.getWebContents() : null;
    }

    // Observer methods.
    private BottomSheetObserver buildBottomSheetObserver() {
        return new EmptyBottomSheetObserver() {
            @Override
            public void onSheetStateChanged(@SheetState int state, @StateChangeReason int reason) {}

            @Override
            public void onSheetClosed(@StateChangeReason int reason) {
                mBottomSheetController.removeObserver(mBottomSheetObserver);
                if (mNativeInterfaceDelegate != null) {
                    mNativeInterfaceDelegate.onBottomSheetClosed();
                    mNativeInterfaceDelegate = null;
                }
                if (mTabBottomSheetCoordinator != null) {
                    mTabBottomSheetCoordinator.destroy();
                    mTabBottomSheetCoordinator = null;
                }
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
