// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
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
                    };
        }

        static NativeInterfaceDelegate getInstance() {
            return LazyHolder.INSTANCE;
        }

        // Method called when the bottom sheet is closed.
        void onBottomSheetClosed();
    }

    private final WindowAndroid mWindowAndroid;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver;

    private @Nullable TabBottomSheetCoordinator mTabBottomSheetCoordinator;
    private @Nullable NativeInterfaceDelegate mNativeInterfaceDelegate;

    /**
     * Constructor.
     *
     * @param windowAndroid The {@link WindowAndroid} for managing window-level operations.
     * @param bottomSheetController The {@link BottomSheetController} used to show the bottom sheet.
     */
    public TabBottomSheetManager(
            WindowAndroid windowAndroid, BottomSheetController bottomSheetController) {
        mWindowAndroid = windowAndroid;
        mBottomSheetController = bottomSheetController;

        mBottomSheetObserver = buildBottomSheetObserver();
        TabBottomSheetUtils.attachManagerToWindow(windowAndroid, this);
    }

    /**
     * Attempts to show the Tab BottomSheet.
     *
     * @param nativeInterfaceDelegate The native interface delegate.
     * @param coBrowseViews The views to show in the bottom sheet.
     * @return Whether the bottom sheet was shown.
     */
    boolean tryToShowBottomSheet(
            NativeInterfaceDelegate nativeInterfaceDelegate, CoBrowseViews coBrowseViews) {
        mTabBottomSheetCoordinator =
                new TabBottomSheetCoordinator(mBottomSheetController, coBrowseViews);

        if (mTabBottomSheetCoordinator.tryToShowBottomSheet()) {
            // Successfully showed bottom sheet.
            mBottomSheetController.addObserver(mBottomSheetObserver);
            mNativeInterfaceDelegate = nativeInterfaceDelegate;
            return true;
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

    // Observer methods.
    private BottomSheetObserver buildBottomSheetObserver() {
        return new EmptyBottomSheetObserver() {
            @Override
            public void onSheetStateChanged(@SheetState int state, @StateChangeReason int reason) {
                if (state == SheetState.HIDDEN) {
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
            }

            @Override
            public void onSheetClosed(@StateChangeReason int reason) {}
        };
    }

    @Override
    public void destroy() {
        if (mTabBottomSheetCoordinator != null) {
            mTabBottomSheetCoordinator.destroy();
            mTabBottomSheetCoordinator = null;
        }
        TabBottomSheetUtils.detachManagerFromWindow(mWindowAndroid);
    }

    /* Testing methods */
    public @Nullable TabBottomSheetCoordinator getTabBottomSheetCoordinatorForTesting() {
        return mTabBottomSheetCoordinator;
    }

    public @Nullable NativeInterfaceDelegate getNativeInterfaceDelegateForTesting() {
        return mNativeInterfaceDelegate;
    }
}
