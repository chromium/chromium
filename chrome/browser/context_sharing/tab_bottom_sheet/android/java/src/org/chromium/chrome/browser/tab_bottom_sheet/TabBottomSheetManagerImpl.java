// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.view.LayoutInflater;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.context_sharing.R;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponentSupplier;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.TouchEventProvider;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

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

    private final LayoutStateObserver mLayoutStateObserver =
            new LayoutStateObserver() {
                @Override
                public void onStartedShowing(@LayoutType int layoutType) {
                    if (layoutType == LayoutType.HUB) {
                        mIsSuppressedOnTabSwitcher = true;
                        maybeCloseBottomSheet();
                    } else if (layoutType == LayoutType.TOOLBAR_SWIPE) {
                        mIsSuppressedOnToolbarSwipe = true;
                        maybeCloseBottomSheet();
                    }
                }

                @Override
                public void onStartedHiding(@LayoutType int layoutType) {
                    if (layoutType == LayoutType.HUB) {
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

    // Indicates if tabBottomSheet was suppressed by entering the tab switcher.
    private boolean mIsSuppressedOnTabSwitcher;
    // Indicates if tabBottomSheet was suppressed by entering the toolbar swipe.
    private boolean mIsSuppressedOnToolbarSwipe;
    // Indicates if tabBottomSheet was suppressed by read aloud.
    private boolean mIsSuppressedByReadAloud;
    // Indicates if tabBottomSheet was suppressed by the user entering incognito mode.
    private boolean mIsSuppressedByIncognito;
    // Indicates if tabBottomSheet was suppressed by autofill keyboard accessory.
    private boolean mIsSuppressedByAutofill;
    // Indicates if tabBottomSheet was suppressed by omnibox focus.
    private boolean mIsSuppressedByOmniboxFocus;
    // Indicates if tabBottomSheet was suppressed by another bottom sheet.
    private boolean mSuppressedByBottomSheetController;

    private boolean isInternallySuppressed() {
        return mIsSuppressedOnTabSwitcher
                || mIsSuppressedOnToolbarSwipe
                || mIsSuppressedByReadAloud
                || mIsSuppressedByIncognito
                || mIsSuppressedByAutofill
                || mIsSuppressedByOmniboxFocus;
    }

    private final NonNullObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private final Callback<Boolean> mOmniboxFocusObserver =
            (hasFocus) -> {
                mIsSuppressedByOmniboxFocus = hasFocus;
                if (hasFocus) {
                    maybeCloseBottomSheet();
                } else {
                    maybeShowBottomSheet();
                }
            };

    private final Callback<TabModel> mTabModelSupplierObserver =
            tabModel -> {
                if (tabModel == null) return;

                boolean isIncognito = tabModel.isIncognito();

                if (isIncognito) {
                    if (!mIsSuppressedByIncognito) {
                        mIsSuppressedByIncognito = true;
                        maybeCloseBottomSheet();
                    }
                } else if (mIsSuppressedByIncognito) {
                    mIsSuppressedByIncognito = false;
                    maybeShowBottomSheet();
                }
            };

    private final TabObserver mTabObserver =
            new EmptyTabObserver() {
                @Override
                public void onDidStartNavigationInPrimaryMainFrame(
                        Tab tab, NavigationHandle navigationHandle) {
                    if (UrlConstants.DISTILLER_SCHEME.equals(
                            navigationHandle.getUrl().getScheme())) {
                        tryToCloseBottomSheet(/* animate= */ false);
                    }
                }
            };
    private @Nullable CurrentTabObserver mCurrentTabObserver;

    private @Nullable TabBottomSheetPeekView mPeekView;
    private @Nullable PeekViewManager mPeekViewManager;
    private @Nullable PropertyModelChangeProcessor mPeekViewChangeProcessor;
    private @Nullable MonotonicObservableSupplier<ManualFillingComponent>
            mManualFillingComponentSupplier;
    private @Nullable ManualFillingComponent mCurrentManualFillingComponent;
    private @Nullable NullableObservableSupplier<Tab> mActivePlaybackTabSupplier;
    private @Nullable Runnable mReadAloudStopPlaybackCallback;
    private final Callback<@Nullable Tab> mActivePlaybackTabObserver =
            this::onActivePlaybackTabChanged;

    private @Nullable TabBottomSheetCoordinator mTabBottomSheetCoordinator;
    private @Nullable NativeInterfaceDelegate mNativeInterfaceDelegate;
    private @Nullable CoBrowseViews mCurrentCoBrowseViews;

    private final Callback<ManualFillingComponent> mFillingComponentObserver =
            this::connectToFillingComponent;

    private final Callback<Boolean> mIsAccessoryRequestedObserver =
            this::onAccessoryRequestedChanged;

    private final Runnable mOnBackPressed = () -> tryToCloseBottomSheet(/* animate= */ true);

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
        mLayoutStateProviderOneShotSupplier = layoutStateProviderOneShotSupplier;
        mTouchEventProvider = touchEventProvider;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;

        mLayoutStateProviderOneShotSupplier.onAvailable(
                mCallbackController.makeCancelable(
                        (provider) -> provider.addObserver(mLayoutStateObserver)));

        MonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                TabModelSelectorSupplier.from(mWindowAndroid);
        if (selectorSupplier != null) {
            selectorSupplier.addSyncObserverAndCallIfNonNull(
                    mCallbackController.makeCancelable(
                            (TabModelSelector selector) -> {
                                if (selector != null) {
                                    selector.getCurrentTabModelSupplier()
                                            .addSyncObserver(mTabModelSupplierObserver);
                                    mCurrentTabObserver =
                                            new CurrentTabObserver(
                                                    selector.getCurrentTabSupplier(), mTabObserver);
                                }
                            }));
        }
        mManualFillingComponentSupplier = ManualFillingComponentSupplier.from(mWindowAndroid);
        if (mManualFillingComponentSupplier != null) {
            mManualFillingComponentSupplier.addSyncObserverAndPostIfNonNull(
                    mFillingComponentObserver);
        }

        mOmniboxFocusStateSupplier.addSyncObserverAndPostIfNonNull(mOmniboxFocusObserver);

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
        clearPeekView();

        mPeekViewManager = coBrowseViews.getPeekViewManager();
        if (mPeekViewManager != null) {
            PropertyModel model = mPeekViewManager.getModel();

            mPeekView =
                    (TabBottomSheetPeekView)
                            LayoutInflater.from(mContext)
                                    .inflate(R.layout.tab_bottom_sheet_peek_layout, null, false);
            mPeekViewChangeProcessor =
                    PropertyModelChangeProcessor.create(
                            model, mPeekView, TabBottomSheetPeekViewBinder::bind);
        }
        mTabBottomSheetCoordinator =
                new TabBottomSheetCoordinator(
                        mContext,
                        mWindowAndroid,
                        mBottomSheetController,
                        mTouchEventProvider,
                        coBrowseViews,
                        mSheetEventsCallback,
                        mOnBackPressed);
        if (mPeekView != null) {
            mTabBottomSheetCoordinator.attachPeekView(mPeekView);
        }

        if (isInternallySuppressed()) {
            // We are currently suppressed, save this sheet to be shown when suppression ends.
            mNativeInterfaceDelegate = nativeInterfaceDelegate;
            if (mIsSuppressedByReadAloud && mReadAloudStopPlaybackCallback != null) {
                mReadAloudStopPlaybackCallback.run();
            }
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
        assert mActivePlaybackTabSupplier == null;
        mActivePlaybackTabSupplier = activePlaybackTabSupplier;
        mActivePlaybackTabSupplier.addSyncObserverAndCallIfNonNull(mActivePlaybackTabObserver);
        mReadAloudStopPlaybackCallback = stopPlaybackCallback;
    }

    @Override
    public void destroy() {
        mOmniboxFocusStateSupplier.removeObserver(mOmniboxFocusObserver);
        if (mActivePlaybackTabSupplier != null) {
            mActivePlaybackTabSupplier.removeObserver(mActivePlaybackTabObserver);
            mActivePlaybackTabSupplier = null;
        }
        mReadAloudStopPlaybackCallback = null;
        if (mCurrentManualFillingComponent != null) {
            mCurrentManualFillingComponent
                    .getIsAccessoryRequestedSupplier()
                    .removeObserver(mIsAccessoryRequestedObserver);
            mCurrentManualFillingComponent = null;
        }
        if (mManualFillingComponentSupplier != null) {
            mManualFillingComponentSupplier.removeObserver(mFillingComponentObserver);
            mManualFillingComponentSupplier = null;
        }

        mState = SheetState.CLOSING;
        mCallbackController.destroy();

        clearPeekView();

        // Destroy the coordinator in case the manager is abruptly destroyed before hiding the
        // bottom sheet.
        if (mTabBottomSheetCoordinator != null) {
            mTabBottomSheetCoordinator.destroy();
            mTabBottomSheetCoordinator = null;
        }
        mCurrentCoBrowseViews = null;
        mState = SheetState.NONE;

        if (mCurrentTabObserver != null) {
            mCurrentTabObserver.destroy();
            mCurrentTabObserver = null;
        }

        TabModelSelector selector = TabModelSelectorSupplier.getValueOrNullFrom(mWindowAndroid);
        if (selector != null) {
            selector.getCurrentTabModelSupplier().removeObserver(mTabModelSupplierObserver);
        }

        var layoutStateProvider = mLayoutStateProviderOneShotSupplier.get();
        if (layoutStateProvider != null) {
            layoutStateProvider.removeObserver(mLayoutStateObserver);
        }

        mNativeInterfaceDelegate = null;
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
        mCurrentCoBrowseViews = null;
        mState = SheetState.NONE;
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
            mState = SheetState.SUPPRESSED;
            mTabBottomSheetCoordinator.closeBottomSheet(/* animate= */ false);
        }
    }

    private void maybeShowBottomSheet() {
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

    private void connectToFillingComponent(ManualFillingComponent component) {
        if (mCurrentManualFillingComponent == component) return;
        if (mCurrentManualFillingComponent != null) {
            mCurrentManualFillingComponent
                    .getIsAccessoryRequestedSupplier()
                    .removeObserver(mIsAccessoryRequestedObserver);
        }

        mCurrentManualFillingComponent = component;
        mCurrentManualFillingComponent
                .getIsAccessoryRequestedSupplier()
                .addSyncObserverAndCallIfNonNull(mIsAccessoryRequestedObserver);
    }

    private void onAccessoryRequestedChanged(boolean isRequested) {
        if (isRequested && !mIsSuppressedByAutofill) {
            mIsSuppressedByAutofill = true;
            maybeCloseBottomSheet();
        } else if (!isRequested && mIsSuppressedByAutofill) {
            mIsSuppressedByAutofill = false;
            maybeShowBottomSheet();
        }
    }

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
        var oldSupplier = mActivePlaybackTabSupplier;
        if (oldSupplier != null) {
            oldSupplier.removeObserver(mActivePlaybackTabObserver);
        }
        var oldCallback = mReadAloudStopPlaybackCallback;

        mActivePlaybackTabSupplier = activePlaybackTabSupplier;
        mActivePlaybackTabSupplier.addSyncObserverAndCallIfNonNull(mActivePlaybackTabObserver);
        mReadAloudStopPlaybackCallback = stopPlaybackCallback;
        ResettersForTesting.register(
                () -> {
                    if (mActivePlaybackTabSupplier != null) {
                        mActivePlaybackTabSupplier.removeObserver(mActivePlaybackTabObserver);
                    }
                    mActivePlaybackTabSupplier = oldSupplier;
                    mReadAloudStopPlaybackCallback = oldCallback;
                    if (mActivePlaybackTabSupplier != null) {
                        mActivePlaybackTabSupplier.addSyncObserverAndCallIfNonNull(
                                mActivePlaybackTabObserver);
                    }
                });
    }

    public void setManualFillingComponentSupplierForTesting(
            @Nullable MonotonicObservableSupplier<ManualFillingComponent> supplier) {
        var oldSupplier = mManualFillingComponentSupplier;
        if (oldSupplier != null) {
            oldSupplier.removeObserver(mFillingComponentObserver);
        }
        mManualFillingComponentSupplier = supplier;
        if (mManualFillingComponentSupplier != null) {
            mManualFillingComponentSupplier.addSyncObserverAndPostIfNonNull(
                    mFillingComponentObserver);
        }
        ResettersForTesting.register(
                () -> {
                    if (mManualFillingComponentSupplier != null) {
                        mManualFillingComponentSupplier.removeObserver(mFillingComponentObserver);
                    }
                    mManualFillingComponentSupplier = oldSupplier;
                    if (mManualFillingComponentSupplier != null) {
                        mManualFillingComponentSupplier.addSyncObserverAndPostIfNonNull(
                                mFillingComponentObserver);
                    } else {
                        if (mCurrentManualFillingComponent != null) {
                            mCurrentManualFillingComponent
                                    .getIsAccessoryRequestedSupplier()
                                    .removeObserver(mIsAccessoryRequestedObserver);
                            mCurrentManualFillingComponent = null;
                        }
                        mIsSuppressedByAutofill = false;
                    }
                });
    }

    private void clearPeekView() {
        if (mPeekViewChangeProcessor != null) {
            mPeekViewChangeProcessor.destroy();
            mPeekViewChangeProcessor = null;
        }
        if (mPeekViewManager != null) {
            mPeekViewManager.destroy();
            mPeekViewManager = null;
        }
        mPeekView = null;
    }
}
