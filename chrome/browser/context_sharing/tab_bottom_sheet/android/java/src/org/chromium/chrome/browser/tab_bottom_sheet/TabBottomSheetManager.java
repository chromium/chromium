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

    private final LayoutStateObserver mLayoutStateObserver =
            new LayoutStateObserver() {
                @Override
                public void onStartedShowing(@LayoutType int layoutType) {
                    if (layoutType == LayoutType.TAB_SWITCHER) {
                        mIsSuppressedOnTabSwitcher = true;
                        maybeCloseBottomSheet();
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
                                maybeShowBottomSheet();
                            }
                        }
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
    private boolean mIsSuppressedByReadAloud;

    private @Nullable NullableObservableSupplier<Tab> mActivePlaybackTabSupplier;
    private final Callback<@Nullable Tab> mActivePlaybackTabObserver =
            this::onActivePlaybackTabChanged;

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
        tryToCloseBottomSheet();
        mTabBottomSheetCoordinator =
                new TabBottomSheetCoordinator(
                        mContext,
                        mWindowAndroid,
                        mBottomSheetController,
                        mTouchEventProvider,
                        coBrowseViews,
                        this::onBottomSheetClosed);

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

    private void onBottomSheetClosed() {
        if (!mIsSuppressedOnTabSwitcher && !mIsSuppressedByReadAloud) {
            notifyOnClose();
        }
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
            mTabBottomSheetCoordinator.closeBottomSheet();
        }
    }

    private void maybeShowBottomSheet() {
        if (!mIsSuppressedOnTabSwitcher && !mIsSuppressedByReadAloud) {

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
