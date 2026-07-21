// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.ui.base.WindowAndroid;

/** Controller responsible for monitoring suppression triggers for the Tab Bottom Sheet. */
@NullMarked
class TabBottomSheetSuppressionController {
    /** Delegate interface to notify the manager when suppression state changes. */
    interface Delegate {
        /** Called when a suppression event requires closing/hiding the bottom sheet. */
        void onSuppressionStarted();

        /** Called when all internal suppressions are cleared and the bottom sheet may reshow. */
        void onSuppressionEnded();

        /** Called when a tab navigation requires closing the bottom sheet explicitly. */
        void onCloseRequested();
    }

    private final WindowAndroid mWindowAndroid;
    private final OneshotSupplier<LayoutStateProvider> mLayoutStateProviderOneShotSupplier;
    private final NonNullObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private final Delegate mDelegate;
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

    private final LayoutStateObserver mLayoutStateObserver;
    private final Callback<Boolean> mOmniboxFocusObserver;
    private final Callback<TabModel> mTabModelSupplierObserver;
    private final TabObserver mTabObserver;

    private @Nullable CurrentTabObserver mCurrentTabObserver;
    private @Nullable MonotonicObservableSupplier<ManualFillingComponent>
            mManualFillingComponentSupplier;
    private @Nullable ManualFillingComponent mCurrentManualFillingComponent;
    private @Nullable NullableObservableSupplier<Tab> mActivePlaybackTabSupplier;
    private @Nullable Runnable mReadAloudStopPlaybackCallback;
    private final Callback<@Nullable Tab> mActivePlaybackTabObserver =
            this::onActivePlaybackTabChanged;

    private final Callback<ManualFillingComponent> mFillingComponentObserver =
            this::connectToFillingComponent;

    private final Callback<Boolean> mIsAccessoryRequestedObserver =
            this::onAccessoryRequestedChanged;

    TabBottomSheetSuppressionController(
            WindowAndroid windowAndroid,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderOneShotSupplier,
            NonNullObservableSupplier<Boolean> omniboxFocusStateSupplier,
            Delegate delegate) {
        mWindowAndroid = windowAndroid;
        mLayoutStateProviderOneShotSupplier = layoutStateProviderOneShotSupplier;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
        mDelegate = delegate;

        mLayoutStateObserver = buildLayoutStateObserver();
        mOmniboxFocusObserver = buildOmniboxFocusObserver();
        mTabModelSupplierObserver = buildTabModelSupplierObserver();
        mTabObserver = buildTabObserver();

        observeLayoutState();
        observeTabModelSelector();
        observeManualFillingComponent();

        mOmniboxFocusStateSupplier.addSyncObserverAndPostIfNonNull(mOmniboxFocusObserver);
    }

    private LayoutStateObserver buildLayoutStateObserver() {
        return new LayoutStateObserver() {
            @Override
            public void onStartedShowing(@LayoutType int layoutType) {
                if (layoutType == LayoutType.HUB) {
                    mIsSuppressedOnTabSwitcher = true;
                    mDelegate.onSuppressionStarted();
                } else if (layoutType == LayoutType.TOOLBAR_SWIPE) {
                    mIsSuppressedOnToolbarSwipe = true;
                    mDelegate.onSuppressionStarted();
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
                    evaluateSuppressionAndNotify();
                }
            }
        };
    }

    private Callback<Boolean> buildOmniboxFocusObserver() {
        return (hasFocus) -> {
            mIsSuppressedByOmniboxFocus = hasFocus;
            if (hasFocus) {
                mDelegate.onSuppressionStarted();
            } else {
                evaluateSuppressionAndNotify();
            }
        };
    }

    private Callback<TabModel> buildTabModelSupplierObserver() {
        return (tabModel) -> {
            if (tabModel == null) return;

            boolean isIncognito = tabModel.isIncognito();

            if (isIncognito) {
                if (!mIsSuppressedByIncognito) {
                    mIsSuppressedByIncognito = true;
                    mDelegate.onSuppressionStarted();
                }
            } else if (mIsSuppressedByIncognito) {
                mIsSuppressedByIncognito = false;
                evaluateSuppressionAndNotify();
            }
        };
    }

    private TabObserver buildTabObserver() {
        return new EmptyTabObserver() {
            @Override
            public void onDidStartNavigationInPrimaryMainFrame(
                    Tab tab, NavigationHandle navigationHandle) {
                if (UrlConstants.DISTILLER_SCHEME.equals(navigationHandle.getUrl().getScheme())) {
                    mDelegate.onCloseRequested();
                }
            }
        };
    }

    boolean isInternallySuppressed() {
        return mIsSuppressedOnTabSwitcher
                || mIsSuppressedOnToolbarSwipe
                || mIsSuppressedByReadAloud
                || mIsSuppressedByIncognito
                || mIsSuppressedByAutofill
                || mIsSuppressedByOmniboxFocus;
    }

    void handleReadAloudStopPlayback() {
        if (mIsSuppressedByReadAloud && mReadAloudStopPlaybackCallback != null) {
            mReadAloudStopPlaybackCallback.run();
        }
    }

    void initReadAloudIntegration(
            NullableObservableSupplier<Tab> activePlaybackTabSupplier,
            Runnable stopPlaybackCallback) {
        assert mActivePlaybackTabSupplier == null;
        mActivePlaybackTabSupplier = activePlaybackTabSupplier;
        mActivePlaybackTabSupplier.addSyncObserverAndCallIfNonNull(mActivePlaybackTabObserver);
        mReadAloudStopPlaybackCallback = stopPlaybackCallback;
    }

    void destroy() {
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

        mCallbackController.destroy();

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
    }

    private void evaluateSuppressionAndNotify() {
        if (!isInternallySuppressed()) {
            mDelegate.onSuppressionEnded();
        }
    }

    private void onActivePlaybackTabChanged(@Nullable Tab tab) {
        if (tab != null) {
            mIsSuppressedByReadAloud = true;
            mDelegate.onSuppressionStarted();
        } else {
            mIsSuppressedByReadAloud = false;
            evaluateSuppressionAndNotify();
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
            mDelegate.onSuppressionStarted();
        } else if (!isRequested && mIsSuppressedByAutofill) {
            mIsSuppressedByAutofill = false;
            evaluateSuppressionAndNotify();
        }
    }

    private void observeLayoutState() {
        mLayoutStateProviderOneShotSupplier.onAvailable(
                mCallbackController.makeCancelable(
                        (provider) -> provider.addObserver(mLayoutStateObserver)));
    }

    private void observeTabModelSelector() {
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
    }

    private void observeManualFillingComponent() {
        mManualFillingComponentSupplier = ManualFillingComponentSupplier.from(mWindowAndroid);
        if (mManualFillingComponentSupplier != null) {
            mManualFillingComponentSupplier.addSyncObserverAndPostIfNonNull(
                    mFillingComponentObserver);
        }
    }

    // TESTING METHODS

    void initReadAloudIntegrationForTesting(
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

    void setManualFillingComponentSupplierForTesting(
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
