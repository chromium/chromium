// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.MonotonicObservableSupplier;
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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.TouchEventProvider;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Implementation of {@link TabBottomSheetManager}. */
@NullMarked
public class TabBottomSheetManagerImpl implements TabBottomSheetManager {
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
    private boolean mIsSuppressedByIncognito;

    private final TabModelSelectorObserver mTabModelSelectorObserver =
            new TabModelSelectorObserver() {
                @Override
                public void onChange() {
                    TabModelSelector selector =
                            TabModelSelectorSupplier.getValueOrNullFrom(mWindowAndroid);
                    if (selector == null) return;

                    boolean isIncognito = selector.isIncognitoSelected();

                    if (isIncognito) {
                        if (!mIsSuppressedByIncognito) {
                            mIsSuppressedByIncognito = true;
                            maybeCloseBottomSheet();
                        }
                    } else {
                        if (mIsSuppressedByIncognito) {
                            mIsSuppressedByIncognito = false;
                            maybeShowBottomSheet();
                        }
                    }
                }
            };
    private boolean mIsSuppressedByAutofill;

    private @Nullable View mPeekView;
    private @Nullable MonotonicObservableSupplier<ManualFillingComponent>
            mManualFillingComponentSupplier;
    private @Nullable ManualFillingComponent mCurrentManualFillingComponent;

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

    private final Callback<ManualFillingComponent> mFillingComponentObserver =
            this::connectToFillingComponent;

    private final Callback<Boolean> mIsAccessoryRequestedObserver =
            this::onAccessoryRequestedChanged;

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
     */
    public TabBottomSheetManagerImpl(
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

        MonotonicObservableSupplier<TabModelSelector> selectorSupplier =
                TabModelSelectorSupplier.from(mWindowAndroid);
        if (selectorSupplier != null) {
            selectorSupplier.addSyncObserverAndCallIfNonNull(
                    mCallbackController.makeCancelable(
                            (TabModelSelector selector) -> {
                                if (selector != null) {
                                    selector.addObserver(mTabModelSelectorObserver);
                                }
                            }));
        }
        mManualFillingComponentSupplier = ManualFillingComponentSupplier.from(mWindowAndroid);
        if (mManualFillingComponentSupplier != null) {
            mManualFillingComponentSupplier.addSyncObserverAndPostIfNonNull(
                    mFillingComponentObserver);
        }

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

        if (mIsSuppressedOnTabSwitcher
                || mIsSuppressedOnToolbarSwipe
                || mIsSuppressedByReadAloud
                || mIsSuppressedByIncognito
                || mIsSuppressedByAutofill) {
            // We are currently suppressed, save this sheet to be shown when suppression ends.
            mNativeInterfaceDelegate = nativeInterfaceDelegate;
            return true;
        }
        if (!mSuppressBottomSheetForTesting
                && mTabBottomSheetCoordinator.tryToShowBottomSheet(animate, startsExpanded)) {
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

    @Override
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

    @Override
    public void setPeekViewModel(PropertyModel model) {
        assert mPeekView == null : "Peek view is already set.";
        TabBottomSheetPeekView peekView =
                (TabBottomSheetPeekView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.tab_bottom_sheet_peek_layout, null, false);
        PropertyModelChangeProcessor.create(model, peekView, TabBottomSheetPeekViewBinder::bind);
        mPeekView = peekView;

        if (mTabBottomSheetCoordinator != null) {
            mTabBottomSheetCoordinator.attachPeekView(mPeekView);
        }
    }

    @Override
    public void removePeekViewModel() {
        // We only support one peek view at a time, so we just remove it if it exists.
        if (mPeekView != null) {
            View peekView = mPeekView;
            mPeekView = null;
            if (mTabBottomSheetCoordinator != null) {
                mTabBottomSheetCoordinator.removePeekView(peekView);
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

        mIsCloseFromNative = true;

        mCallbackController.destroy();

        // Destroy the coordinator in case the manager is abruptly destroyed before hiding the
        // bottom sheet.
        if (mTabBottomSheetCoordinator != null) {
            mTabBottomSheetCoordinator.destroy();
            mTabBottomSheetCoordinator = null;
        }

        TabModelSelector selector = TabModelSelectorSupplier.getValueOrNullFrom(mWindowAndroid);
        if (selector != null) {
            selector.removeObserver(mTabModelSelectorObserver);
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
                && !mIsSuppressedByReadAloud
                && !mIsSuppressedByIncognito
                && !mIsSuppressedByAutofill) {

            if (mTabBottomSheetCoordinator != null && mNativeInterfaceDelegate != null) {
                if (!mTabBottomSheetCoordinator.tryToShowBottomSheet(
                        /* animate= */ false, /* startsExpanded= */ false)) {
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

    private boolean mSuppressBottomSheetForTesting;

    public void suppressBottomSheetForTesting(boolean suppress) {
        mSuppressBottomSheetForTesting = suppress;
    }

    public void setReadAloudActivePlaybackTabSupplierForTesting(
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
}
