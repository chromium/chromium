// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.TouchEventProvider;
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
                    if (mNativeInterfaceDelegate == null) {
                        return;
                    }
                    if (mIsCloseFromNative) {
                        notifyOnClose();
                    } else {
                        mNativeInterfaceDelegate.onBottomSheetSuppressed();
                    }
                }

                @Override
                public void onBottomSheetOpened(boolean isExpanded) {
                    if (mNativeInterfaceDelegate == null) {
                        return;
                    }
                    mNativeInterfaceDelegate.onBottomSheetOpened(isExpanded);
                }
            };

    private final LayoutStateObserver mLayoutStateObserver =
            new LayoutStateObserver() {
                @Override
                public void onStartedShowing(@LayoutType int layoutType) {
                    if (layoutType == LayoutType.TAB_SWITCHER) {
                        mIsSuppressedOnTabSwitcher = true;
                        maybeCloseBottomSheet();
                    } else if (layoutType == LayoutType.TOOLBAR_SWIPE) {
                        mIsSuppressedOnToolbarSwipe = true;
                        maybeCloseBottomSheet();
                    }
                }

                @Override
                public void onStartedHiding(@LayoutType int layoutType) {
                    if (layoutType == LayoutType.TAB_SWITCHER) {
                        mIsSuppressedOnTabSwitcher = false;
                        maybeShowIfNextIsBrowsing();
                    } else if (layoutType == LayoutType.TOOLBAR_SWIPE) {
                        mIsSuppressedOnToolbarSwipe = false;
                        maybeShowIfNextIsBrowsing();
                    }
                }

                private void maybeShowIfNextIsBrowsing() {
                    var layoutStateProvider = mLayoutStateProviderOneShotSupplier.get();
                    assert layoutStateProvider != null;
                    @LayoutType int nextLayoutType = layoutStateProvider.getNextLayoutType();
                    if (nextLayoutType == LayoutType.BROWSING) {
                        maybeShowBottomSheet();
                    }
                }
            };

    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final BottomSheetController mBottomSheetController;
    private final OneshotSupplier<LayoutStateProvider> mLayoutStateProviderOneShotSupplier;
    private final TouchEventProvider mTouchEventProvider;
    private final CallbackController mCallbackController = new CallbackController();

    private boolean mIsSuppressedOnTabSwitcher;
    private boolean mIsSuppressedOnToolbarSwipe;
    private boolean mIsSuppressedByReadAloud;

    private @Nullable View mPeekView;
    private @Nullable NullableObservableSupplier<Tab> mActivePlaybackTabSupplier;
    private final Callback<@Nullable Tab> mActivePlaybackTabObserver =
            this::onActivePlaybackTabChanged;

    // The bottom sheet can only be closed through a native event or when this manager is destroyed.
    // If the bottom sheet was ever hidden, while this boolean is false, we assume that the bottom
    // sheet had been suppressed and that it will be shown again once the suppression event passes.
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
     * @param touchEventProvider The {@link TouchEventProvider} used to observe touch events on the
     *     tab behind the bottom sheet.
     */
    public TabBottomSheetManager(
            Context context,
            WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderOneShotSupplier,
            TouchEventProvider touchEventProvider) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mBottomSheetController = bottomSheetController;
        mLayoutStateProviderOneShotSupplier = layoutStateProviderOneShotSupplier;
        mTouchEventProvider = touchEventProvider;

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
        tryToCloseBottomSheet(/* animate= */ false);
        mTabBottomSheetCoordinator =
                new TabBottomSheetCoordinator(
                        mContext,
                        mWindowAndroid,
                        mBottomSheetController,
                        mTouchEventProvider,
                        coBrowseViews,
                        mSheetEventsCallback);
        if (mPeekView != null) {
            mTabBottomSheetCoordinator.attachPeekView(mPeekView);
        }

        if (mIsSuppressedOnTabSwitcher || mIsSuppressedOnToolbarSwipe || mIsSuppressedByReadAloud) {
            // We are currently suppressed, save this sheet to be shown when suppression ends.
            mNativeInterfaceDelegate = nativeInterfaceDelegate;
            return true;
        }
        if (mTabBottomSheetCoordinator.tryToShowBottomSheet(animate, startsExpanded)) {
            // Successfully showed bottom sheet.
            mNativeInterfaceDelegate = nativeInterfaceDelegate;
            return true;
        }
        // Failed to show bottom sheet, remove it from queue.
        mTabBottomSheetCoordinator.closeBottomSheet(/* animate= */ false);
        return false;
    }

    void detachNativeInterfaceDelegate(NativeInterfaceDelegate delegate) {
        if (mNativeInterfaceDelegate == delegate) {
            mNativeInterfaceDelegate = null;
        }
    }

    /** Attempts to close the Tab BottomSheet. */
    public void tryToCloseBottomSheet(boolean animate) {
        if (mTabBottomSheetCoordinator != null) {
            if (!mTabBottomSheetCoordinator.isSheetShowing()) {
                // The bottom sheet is already closed. just send a onClose event back to native.
                notifyOnClose();
            } else {
                // The bottom sheet is showing. Close it and send a onClose event back to native.
                mIsCloseFromNative = true;
                mTabBottomSheetCoordinator.closeBottomSheet(animate);
            }
        }
    }

    /**
     * Sets the peek view to be displayed.
     *
     * @param peekView The peek view to be displayed.
     */
    public void setPeekView(View peekView) {
        assert mPeekView == null : "Peek view is already set.";
        mPeekView = peekView;

        if (mTabBottomSheetCoordinator != null) {
            mTabBottomSheetCoordinator.attachPeekView(mPeekView);
        }
    }

    /**
     * Removes the peek view if it matches the provided view.
     *
     * @param peekView The peek view to be removed.
     */
    public void removePeekView(View peekView) {
        if (mPeekView == peekView) {
            mPeekView = null;
            if (mTabBottomSheetCoordinator != null) {
                mTabBottomSheetCoordinator.removePeekView(peekView);
            }
        }
    }

    /**
     * Sets whether the bottom sheet is expanded.
     *
     * @param expanded Whether the bottom sheet should be expanded.
     */
    public void setSheetExpanded(boolean expanded) {
        if (mTabBottomSheetCoordinator != null) {
            mTabBottomSheetCoordinator.setSheetExpanded(expanded);
        }
    }

    public boolean isSheetInitialized() {
        return mTabBottomSheetCoordinator != null;
    }

    boolean isSheetShowing() {
        return mTabBottomSheetCoordinator != null && mTabBottomSheetCoordinator.isSheetShowing();
    }

    /**
     * Sets the supplier for the active playback tab from ReadAloud.
     *
     * @param activePlaybackTabSupplier The supplier.
     */
    public void setReadAloudActivePlaybackTabSupplier(
            NullableObservableSupplier<Tab> activePlaybackTabSupplier) {
        assert mActivePlaybackTabSupplier == null;
        mActivePlaybackTabSupplier = activePlaybackTabSupplier;
        mActivePlaybackTabSupplier.addSyncObserverAndCallIfNonNull(mActivePlaybackTabObserver);
    }

    @Override
    public void destroy() {
        if (mActivePlaybackTabSupplier != null) {
            mActivePlaybackTabSupplier.removeObserver(mActivePlaybackTabObserver);
            mActivePlaybackTabSupplier = null;
        }
        mIsCloseFromNative = true;

        mCallbackController.destroy();

        // Destroy the coordinator in case the manager is abruptly destroyed before hiding the
        // bottom sheet.
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
        // The only time the sheet isn't destroyed is if we enter the tab switcher or read aloud is
        // playing, in which case we close the sheet but hold only the coordinator to reshow the
        // sheet if we return to the same tab or read aloud stops.
        if (mTabBottomSheetCoordinator != null) {
            mTabBottomSheetCoordinator.destroy();
            mTabBottomSheetCoordinator = null;
        }
        mIsCloseFromNative = false;
    }

    private void onActivePlaybackTabChanged(@Nullable Tab tab) {
        if (tab != null) {
            mIsSuppressedByReadAloud = true;
            maybeCloseBottomSheet();
        } else {
            mIsSuppressedByReadAloud = false;
            maybeShowBottomSheet();
        }
    }

    private void maybeCloseBottomSheet() {
        if (mTabBottomSheetCoordinator != null && mNativeInterfaceDelegate != null) {
            mTabBottomSheetCoordinator.closeBottomSheet(/* animate= */ false);
        }
    }

    private void maybeShowBottomSheet() {
        if (!mIsSuppressedOnTabSwitcher
                && !mIsSuppressedOnToolbarSwipe
                && !mIsSuppressedByReadAloud) {

            if (mTabBottomSheetCoordinator != null && mNativeInterfaceDelegate != null) {
                if (!mTabBottomSheetCoordinator.tryToShowBottomSheet(
                        /* animate= */ false, /* startsExpanded= */ false)) {
                    notifyOnClose();
                }
            }
        }
    }

    /* Testing methods */
    public @Nullable TabBottomSheetCoordinator getTabBottomSheetCoordinatorForTesting() {
        return mTabBottomSheetCoordinator;
    }

    public @Nullable NativeInterfaceDelegate getNativeInterfaceDelegateForTesting() {
        return mNativeInterfaceDelegate;
    }

    void setReadAloudActivePlaybackTabSupplierForTesting(
            NullableObservableSupplier<Tab> activePlaybackTabSupplier) {
        var oldSupplier = mActivePlaybackTabSupplier;
        if (oldSupplier != null) {
            oldSupplier.removeObserver(mActivePlaybackTabObserver);
        }

        mActivePlaybackTabSupplier = activePlaybackTabSupplier;
        mActivePlaybackTabSupplier.addSyncObserverAndCallIfNonNull(mActivePlaybackTabObserver);
        ResettersForTesting.register(
                () -> {
                    if (mActivePlaybackTabSupplier != null) {
                        mActivePlaybackTabSupplier.removeObserver(mActivePlaybackTabObserver);
                    }
                    mActivePlaybackTabSupplier = oldSupplier;
                    if (mActivePlaybackTabSupplier != null) {
                        mActivePlaybackTabSupplier.addSyncObserverAndCallIfNonNull(
                                mActivePlaybackTabObserver);
                    }
                });
    }
}
