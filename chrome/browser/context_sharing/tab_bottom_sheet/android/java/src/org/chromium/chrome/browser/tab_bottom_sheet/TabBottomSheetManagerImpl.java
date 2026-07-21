// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.view.View;
import android.view.Window;

import androidx.annotation.IntDef;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.TouchEventProvider;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Implementation of {@link TabBottomSheetManager}. */
@NullMarked
public class TabBottomSheetManagerImpl implements TabBottomSheetManager {
    /** Represents the logical states of the bottom sheet manager's lifecycle. */
    @IntDef({SheetState.NONE, SheetState.SHOWING, SheetState.SUPPRESSED, SheetState.CLOSING})
    @Retention(RetentionPolicy.SOURCE)
    private @interface SheetState {
        /** No active bottom sheet session. Implies coordinator, delegate, and views are null. */
        int NONE = 0;

        /**
         * Sheet is actively showing (peek or expanded). Implies coordinator, delegate, and views
         * are non-null.
         */
        int SHOWING = 1;

        /**
         * Sheet is temporarily hidden due to suppression. Coordinator is kept alive to preserve
         * CoBrowseViews.
         */
        int SUPPRESSED = 2;

        /**
         * Sheet is animating closed for explicit teardown. Coordinator is kept alive until
         * animation ends.
         */
        int CLOSING = 3;
    }

    private @SheetState int mState = SheetState.NONE;

    private final TabBottomSheetCoordinator.SheetEventsCallback mSheetEventsCallback =
            new TabBottomSheetCoordinator.SheetEventsCallback() {
                @Override
                public void onBottomSheetClosed() {
                    if (mNativeInterfaceDelegate == null) {
                        return;
                    }
                    if (mState == SheetState.CLOSING) {
                        notifyOnClose();
                    } else {
                        mSuppressedByBottomSheetController = !isInternallySuppressed();
                        mNativeInterfaceDelegate.onBottomSheetSuppressed();
                    }
                }

                @Override
                public void onBottomSheetOpened(boolean isExpanded) {
                    mSuppressedByBottomSheetController = false;
                    if (mNativeInterfaceDelegate == null) {
                        return;
                    }
                    mNativeInterfaceDelegate.onBottomSheetOpened(isExpanded);
                }
            };

    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final BottomSheetController mBottomSheetController;
    private final TouchEventProvider mTouchEventProvider;
    private final TabBottomSheetSuppressionController mSuppressionController;

    // Indicates if tabBottomSheet was suppressed by another bottom sheet.
    private boolean mSuppressedByBottomSheetController;

    private @Nullable TabBottomSheetCoordinator mTabBottomSheetCoordinator;
    private @Nullable NativeInterfaceDelegate mNativeInterfaceDelegate;
    private @Nullable CoBrowseViews mCurrentCoBrowseViews;

    private final Runnable mOnBackPressed = () -> tryToCloseBottomSheet(/* animate= */ true);

    private final TabBottomSheetSuppressionController.Delegate mSuppressionDelegate =
            new TabBottomSheetSuppressionController.Delegate() {
                @Override
                public void onSuppressionStarted() {
                    suppressBottomSheet();
                }

                @Override
                public void onSuppressionEnded() {
                    unsuppressBottomSheet();
                }

                @Override
                public void onCloseRequested() {
                    tryToCloseBottomSheet(/* animate= */ false);
                }
            };

    /**
     * Constructor.
     *
     * @param context Context.
     * @param windowAndroid The {@link WindowAndroid} for managing window-level operations.
     * @param bottomSheetController The {@link BottomSheetController} used to show the bottom sheet.
     * @param layoutStateProviderOneShotSupplier The {@link LayoutStateProvider} for managing layout
     *     state.
     * @param touchEventProvider The {@link TouchEventProvider} used to observe touch events on the
     *     tab behind the bottom sheet.
     * @param omniboxFocusStateSupplier The {@link NonNullObservableSupplier} for the omnibox focus
     *     state.
     */
    public TabBottomSheetManagerImpl(
            Context context,
            WindowAndroid windowAndroid,
            BottomSheetController bottomSheetController,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderOneShotSupplier,
            TouchEventProvider touchEventProvider,
            NonNullObservableSupplier<Boolean> omniboxFocusStateSupplier) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mBottomSheetController = bottomSheetController;
        mTouchEventProvider = touchEventProvider;

        mSuppressionController =
                new TabBottomSheetSuppressionController(
                        windowAndroid,
                        layoutStateProviderOneShotSupplier,
                        omniboxFocusStateSupplier,
                        mSuppressionDelegate);

        TabBottomSheetUtils.attachManagerToWindow(mWindowAndroid, this);
        notifyNativeManagerInitialized();
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
        if (mState == SheetState.SHOWING
                && mNativeInterfaceDelegate == nativeInterfaceDelegate
                && mCurrentCoBrowseViews == coBrowseViews) {
            setSheetExpanded(startsExpanded);
            return true;
        }
        // If a native close is in progress, synchronously finish it before opening the new one.
        if (mState == SheetState.CLOSING) {
            tryToCloseBottomSheet(/* animate= */ false);
        }
        // Close any existing bottom sheet before showing a new one.
        tryToCloseBottomSheet(/* animate= */ false);

        mTabBottomSheetCoordinator =
                new TabBottomSheetCoordinator(
                        mContext,
                        mWindowAndroid,
                        mBottomSheetController,
                        mTouchEventProvider,
                        coBrowseViews,
                        mSheetEventsCallback,
                        mOnBackPressed);

        if (isInternallySuppressed()) {
            // We are currently suppressed, save this sheet to be shown when suppression ends.
            mNativeInterfaceDelegate = nativeInterfaceDelegate;
            mSuppressionController.handleReadAloudStopPlayback();
            return true;
        }
        if (!mSuppressBottomSheetForTesting
                && mTabBottomSheetCoordinator.tryToShowBottomSheet(animate, startsExpanded)) {
            // Successfully showed bottom sheet.
            mNativeInterfaceDelegate = nativeInterfaceDelegate;
            mCurrentCoBrowseViews = coBrowseViews;
            mState = SheetState.SHOWING;
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

    @Override
    public void tryToCloseBottomSheet(boolean animate) {
        if (mTabBottomSheetCoordinator != null) {
            if (mSuppressedByBottomSheetController) {
                // BottomSheet is closed but still in queue.
                mSuppressedByBottomSheetController = false;
                mState = SheetState.CLOSING;
                mTabBottomSheetCoordinator.closeBottomSheet(animate);
                notifyOnClose();
            } else if (!mTabBottomSheetCoordinator.isSheetShowing()) {
                // The bottom sheet is already closed. just send a onClose event back to native.
                notifyOnClose();
            } else {
                // The bottom sheet is showing. Close it and send a onClose event back to native.
                mState = SheetState.CLOSING;
                mTabBottomSheetCoordinator.closeBottomSheet(animate);
            }
        }
    }

    @Override
    public void setSheetExpanded(boolean expanded) {
        if (mTabBottomSheetCoordinator != null) {
            mTabBottomSheetCoordinator.setSheetExpanded(expanded);
        }
    }

    @Override
    public boolean isSheetInitialized() {
        return mTabBottomSheetCoordinator != null;
    }

    @Override
    public boolean isSheetShowing() {
        return mTabBottomSheetCoordinator != null && mTabBottomSheetCoordinator.isSheetShowing();
    }

    @Override
    public boolean isInPeekMode() {
        return mTabBottomSheetCoordinator != null
                && mTabBottomSheetCoordinator.isSheetShowing()
                && mTabBottomSheetCoordinator.isInPeekMode();
    }

    @Override
    public void initReadAloudIntegration(
            NullableObservableSupplier<Tab> activePlaybackTabSupplier,
            Runnable stopPlaybackCallback) {
        mSuppressionController.initReadAloudIntegration(
                activePlaybackTabSupplier, stopPlaybackCallback);
    }

    @Override
    public void destroy() {
        mSuppressionController.destroy();

        mState = SheetState.CLOSING;

        // Destroy the coordinator in case the manager is abruptly destroyed before hiding the
        // bottom sheet.
        if (mTabBottomSheetCoordinator != null) {
            mTabBottomSheetCoordinator.destroy();
            mTabBottomSheetCoordinator = null;
        }
        mCurrentCoBrowseViews = null;
        mState = SheetState.NONE;

        mNativeInterfaceDelegate = null;
        TabBottomSheetUtils.detachManagerFromWindow(mWindowAndroid);
    }

    private boolean isInternallySuppressed() {
        return mSuppressionController.isInternallySuppressed();
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
        mCurrentCoBrowseViews = null;
        mState = SheetState.NONE;
    }

    private void suppressBottomSheet() {
        if (mTabBottomSheetCoordinator != null && mNativeInterfaceDelegate != null) {
            mState = SheetState.SUPPRESSED;
            mTabBottomSheetCoordinator.closeBottomSheet(/* animate= */ false);
        }
    }

    private void unsuppressBottomSheet() {
        if (!isInternallySuppressed()) {
            if (mTabBottomSheetCoordinator != null && mNativeInterfaceDelegate != null) {
                if (mState == SheetState.SHOWING) return;
                if (mTabBottomSheetCoordinator.tryToShowBottomSheet(
                        /* animate= */ false, /* startsExpanded= */ false)) {
                    mState = SheetState.SHOWING;
                } else {
                    notifyOnClose();
                }
            }
        }
    }

    private void notifyNativeManagerInitialized() {
        // Defer sending onManagerInitialized to C++ until the activity's window has non-zero
        // height. During early activity restoration/recreation, layout passes haven't run yet
        // (decorView.getHeight() == 0). Sending the initialization event after layout pass
        // guarantees full layout height is available when C++ calculates bottom sheet dimensions.
        Window window = mWindowAndroid.getWindow();
        if (window != null) {
            View decorView = window.getDecorView();
            if (decorView.getHeight() > 0) {
                TabBottomSheetNativeInterfaceJni.get().onManagerInitialized(mWindowAndroid);
            } else {
                decorView.addOnLayoutChangeListener(
                        new View.OnLayoutChangeListener() {
                            @Override
                            public void onLayoutChange(
                                    View v,
                                    int left,
                                    int top,
                                    int right,
                                    int bottom,
                                    int oldLeft,
                                    int oldTop,
                                    int oldRight,
                                    int oldBottom) {
                                if (decorView.getHeight() > 0) {
                                    decorView.removeOnLayoutChangeListener(this);
                                    if (TabBottomSheetUtils.getManagerFromWindow(mWindowAndroid)
                                            != null) {
                                        TabBottomSheetNativeInterfaceJni.get()
                                                .onManagerInitialized(mWindowAndroid);
                                    }
                                }
                            }
                        });
            }
        } else {
            TabBottomSheetNativeInterfaceJni.get().onManagerInitialized(mWindowAndroid);
        }
    }

    // TESTING METHODS

    public @Nullable TabBottomSheetCoordinator getTabBottomSheetCoordinatorForTesting() {
        return mTabBottomSheetCoordinator;
    }

    public @Nullable NativeInterfaceDelegate getNativeInterfaceDelegateForTesting() {
        return mNativeInterfaceDelegate;
    }

    public void attachNativeInterfaceDelegateForTesting(NativeInterfaceDelegate delegate) {
        mNativeInterfaceDelegate = delegate;
    }

    private boolean mSuppressBottomSheetForTesting;

    public void suppressBottomSheetForTesting(boolean suppress) {
        mSuppressBottomSheetForTesting = suppress;
    }

    public void initReadAloudIntegrationForTesting(
            NullableObservableSupplier<Tab> activePlaybackTabSupplier,
            Runnable stopPlaybackCallback) {
        mSuppressionController.initReadAloudIntegrationForTesting(
                activePlaybackTabSupplier, stopPlaybackCallback);
    }

    public void setManualFillingComponentSupplierForTesting(
            @Nullable MonotonicObservableSupplier<ManualFillingComponent> supplier) {
        mSuppressionController.setManualFillingComponentSupplierForTesting(supplier);
    }
}
