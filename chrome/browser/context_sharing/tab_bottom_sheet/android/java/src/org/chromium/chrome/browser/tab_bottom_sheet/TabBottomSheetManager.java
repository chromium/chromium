// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.view.View;

import org.chromium.base.CallbackController;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.WindowAndroid;

/** Manager class for the tab bottom sheet. */
@NullMarked
public class TabBottomSheetManager implements Destroyable {

    // Interface for the native to communicate with the tab bottom sheet manager.
    interface NativeInterfaceDelegate {
        // Method called when the bottom sheet is closed.
        void onBottomSheetClosed();

        // Called when the bottom sheet is opened, or when the bottom sheet state changes.
        void onBottomSheetOpened(boolean isExpanded);

        // Method called when the bottom sheet is suppressed.
        void onBottomSheetSuppressed();
    }

    private final TabBottomSheetCoordinator.SheetEventsCallback mSheetEventsCallback =
            new TabBottomSheetCoordinator.SheetEventsCallback() {
                @Override
                public void onBottomSheetClosed() {
                    if (mNativeInterfaceDelegate == null) return;
                    if (mIsCloseFromNative) {
                        notifyOnClose();
                    } else {
                        mNativeInterfaceDelegate.onBottomSheetSuppressed();
                    }
                }

                @Override
                public void onBottomSheetOpened(boolean isExpanded) {
                    if (mNativeInterfaceDelegate == null) return;
                    mNativeInterfaceDelegate.onBottomSheetOpened(isExpanded);
                }
            };

    private final LayoutStateObserver mLayoutStateObserver =
            new LayoutStateObserver() {
                @Override
                public void onStartedShowing(@LayoutType int layoutType) {
                    if (layoutType == LayoutType.TAB_SWITCHER) {
                        mIsSuppressedOnTabSwitcher = true;
                        if (mTabBottomSheetCoordinator != null
                                && mNativeInterfaceDelegate != null) {
                            mTabBottomSheetCoordinator.closeBottomSheet();
                        }
                    }
                }

                @Override
                public void onStartedHiding(@LayoutType int layoutType) {
                    if (layoutType == LayoutType.TAB_SWITCHER) {
                        mIsSuppressedOnTabSwitcher = false;
                        if (mLayoutStateProviderOneShotSupplier.get() != null) {
                            @LayoutType
                            int nextLayoutType =
                                    mLayoutStateProviderOneShotSupplier.get().getNextLayoutType();
                            if (nextLayoutType == LayoutType.BROWSING) {
                                if (mTabBottomSheetCoordinator != null
                                        && mNativeInterfaceDelegate != null) {
                                    if (!mTabBottomSheetCoordinator.tryToShowBottomSheet(
                                            /* animate= */ false, /* startsExpanded= */ false)) {
                                        notifyOnClose();
                                    }
                                }
                            }
                        }
                    }
                }
            };

    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final BottomSheetController mBottomSheetController;
    private final OneshotSupplier<LayoutStateProvider> mLayoutStateProviderOneShotSupplier;
    private final CallbackController mCallbackController = new CallbackController();

    private boolean mIsSuppressedOnTabSwitcher;
    // The bottom sheet can only be closed through a native event, if the bottom sheet was ever
    // hidden, while this boolean is false, we assume that the bottom sheet had been suppressed and
    // that it will be shown again once the suppression event passes.
    // When it is true, the close event originated from native, we close the bottom sheet, send an
    // onClosed event to native, and reset the boolean to false.
    private boolean mIsCloseFromNative;

    private @Nullable TabBottomSheetCoordinator mTabBottomSheetCoordinator;
    private @Nullable NativeInterfaceDelegate mNativeInterfaceDelegate;

    /**
     * Constructor.
     *
     * @param context Context.
     * @param windowAndroid The {@link WindowAndroid} for managing window-level operations.
     * @param bottomSheetController The {@link BottomSheetController} used to show the bottom sheet.
     * @param tabModelSelector The {@link TabModelSelector} for managing tab models.
     * @param layoutStateProviderOneShotSupplier The {@link LayoutStateProvider} for managing layout
     *     state.
     */
    public TabBottomSheetManager(
            Context context,
            WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderOneShotSupplier) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mBottomSheetController = bottomSheetController;
        mLayoutStateProviderOneShotSupplier = layoutStateProviderOneShotSupplier;

        mLayoutStateProviderOneShotSupplier.onAvailable(
                mCallbackController.makeCancelable(
                        (provider) -> provider.addObserver(mLayoutStateObserver)));

        TabBottomSheetUtils.attachManagerToWindow(windowAndroid, this);
    }

    /**
     * Attempts to show the Tab BottomSheet.
     *
     * @param nativeInterfaceDelegate The native interface delegate.
     * @param coBrowseViews The views to be displayed within the bottom sheet. These should be
     *     obtained via {@link CoBrowseViewFactory}. Note that these views have a single-use
     *     lifecycle; they are destroyed when the bottom sheet is closed and cannot be reused for
     *     subsequent showings.
     * @param animate Whether to animate the bottom sheet.
     * @param startsExpanded Whether the bottom sheet should start in the expanded state.
     * @return Whether the bottom sheet was shown.
     */
    boolean tryToShowBottomSheet(
            NativeInterfaceDelegate nativeInterfaceDelegate,
            CoBrowseViews coBrowseViews,
            boolean animate,
            boolean startsExpanded) {
        // Close any existing bottom sheet before showing a new one.
        tryToCloseBottomSheet();
        mTabBottomSheetCoordinator =
                new TabBottomSheetCoordinator(
                        mContext,
                        mWindowAndroid,
                        mBottomSheetController,
                        coBrowseViews,
                        mSheetEventsCallback);

        if (mIsSuppressedOnTabSwitcher) {
            // We are currently in the tab switcher, save this sheet to be shown when we return to a
            // tab.
            mNativeInterfaceDelegate = nativeInterfaceDelegate;
            return true;
        }
        if (mTabBottomSheetCoordinator.tryToShowBottomSheet(animate, startsExpanded)) {
            // Successfully showed bottom sheet.
            mNativeInterfaceDelegate = nativeInterfaceDelegate;
            return true;
        }
        // Failed to show bottom sheet.
        mTabBottomSheetCoordinator.destroy();
        mTabBottomSheetCoordinator = null;
        return false;
    }

    void detachNativeInterfaceDelegate(NativeInterfaceDelegate delegate) {
        if (mNativeInterfaceDelegate == delegate) {
            mNativeInterfaceDelegate = null;
        }
    }

    void tryToCloseBottomSheet() {
        if (mTabBottomSheetCoordinator != null) {
            if (!mTabBottomSheetCoordinator.isSheetShowing()) {
                // The bottom sheet is already closed. just send a onClose event back to native.
                notifyOnClose();
            } else {
                mIsCloseFromNative = true;
                mTabBottomSheetCoordinator.closeBottomSheet();
            }
        }
    }

    /**
     * Attaches the peek view to the bottom sheet.
     *
     * @param peekView The peek view to attach.
     */
    public void attachPeekView(View peekView) {
        if (mTabBottomSheetCoordinator != null) {
            mTabBottomSheetCoordinator.attachPeekView(peekView);
        }
    }

    /**
     * Shows the peek view from the bottom sheet.
     *
     * @return Whether the peek view was successfully shown.
     */
    public boolean showPeekView() {
        if (mTabBottomSheetCoordinator != null) {
            return mTabBottomSheetCoordinator.showPeekViewAndHideExpandedContent();
        }
        return false;
    }

    /**
     * Hides the peek view from the bottom sheet.
     *
     * @return Whether the peek view was successfully hidden.
     */
    public boolean hidePeekView() {
        if (mTabBottomSheetCoordinator != null) {
            return mTabBottomSheetCoordinator.hidePeekViewAndShowExpandedContent();
        }
        return false;
    }

    public boolean isSheetInitialized() {
        return mTabBottomSheetCoordinator != null;
    }

    boolean isSheetShowing() {
        return mTabBottomSheetCoordinator != null && mTabBottomSheetCoordinator.isSheetShowing();
    }

    @Override
    public void destroy() {
        mIsCloseFromNative = true;

        mCallbackController.destroy();

        // Destroy the coorinator in case the manager is abruptly destroyed before hiding the bottom
        // sheet.
        if (mTabBottomSheetCoordinator != null) {
            mTabBottomSheetCoordinator.destroy();
            mTabBottomSheetCoordinator = null;
        }

        var layoutStateProvider = mLayoutStateProviderOneShotSupplier.get();
        if (layoutStateProvider != null) {
            layoutStateProvider.removeObserver(mLayoutStateObserver);
        }

        TabBottomSheetUtils.detachManagerFromWindow(mWindowAndroid);
    }

    private void notifyOnClose() {
        if (mNativeInterfaceDelegate != null) {
            mNativeInterfaceDelegate.onBottomSheetClosed();
            mNativeInterfaceDelegate = null;
        }
        // Destroy the sheet after notifying native of the close event.
        // The only time the sheet isn't destroyed is if we enter the tab switcher, in which case
        // we close the sheet but hold only the coordinator to reshow the sheet if we return to the
        // same tab.
        if (mTabBottomSheetCoordinator != null) {
            mTabBottomSheetCoordinator.destroy();
            mTabBottomSheetCoordinator = null;
        }
        mIsCloseFromNative = false;
    }

    /* Testing methods */
    public @Nullable TabBottomSheetCoordinator getTabBottomSheetCoordinatorForTesting() {
        return mTabBottomSheetCoordinator;
    }

    public @Nullable NativeInterfaceDelegate getNativeInterfaceDelegateForTesting() {
        return mNativeInterfaceDelegate;
    }
}
