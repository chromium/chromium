// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.chrome.features.start_surface.StartSurface.StateObserver;
import org.chromium.chrome.features.start_surface.StartSurfaceState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.util.TokenHolder;
import org.chromium.ui.vr.VrModeObserver;
import org.chromium.url.GURL;

/**
 * A class that manages activity-specific interactions with the BottomSheet component that it
 * otherwise shouldn't know about.
 */
class BottomSheetManager extends EmptyBottomSheetObserver implements DestroyObserver {
    /** A means of accessing the focus state of the omibox. */
    private final ObservableSupplier<Boolean> mOmniboxFocusStateSupplier;

    /** An observer of the omnibox that suppresses the sheet when the omnibox is focused. */
    private final Callback<Boolean> mOmniboxFocusObserver;

    /** A {@link VrModeObserver} that observers events of entering and exiting VR mode. */
    private final VrModeObserver mVrModeObserver;

    /** A listener for browser controls offset changes. */
    private final BrowserControlsVisibilityManager.Observer mBrowserControlsObserver;

    /** A tab observer that is only attached to the active tab. */
    private final TabObserver mTabObserver;

    private final CallbackController mCallbackController;

    /** The supplier of {@link StartSurface} instance. */
    private final OneshotSupplier<StartSurface> mStartSurfaceSupplier;
    private StateObserver mStartSurfaceStateObserver;

    private final boolean mIsStartSurfaceRefactorEnabled;
    private final OneshotSupplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private LayoutStateProvider.LayoutStateObserver mLayoutStateObserver;

    /** A browser controls manager for polling browser controls offsets. */
    private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;

    /** A token for suppressing app modal dialogs. */
    private int mAppModalToken = TokenHolder.INVALID_TOKEN;

    /** A token for suppressing tab modal dialogs. */
    private int mTabModalToken = TokenHolder.INVALID_TOKEN;

    /**
     * A handle to the {@link ManagedBottomSheetController} this class manages interactions with.
     */
    private ManagedBottomSheetController mSheetController;

    /** A mechanism for accessing the currently active tab. */
    private ActivityTabProvider mTabProvider;

    /** A supplier of the activity's dialog manager. */
    private Supplier<ModalDialogManager> mDialogManager;

    /** A supplier of a snackbar manager for the bottom sheet. */
    private Supplier<SnackbarManager> mSnackbarManager;

    /** A delegate that provides the functionality of obscuring all tabs. */
    private TabObscuringHandler mTabObscuringHandler;

    /** A token held while the bottom sheet is obscuring all visible tabs. */
    private TabObscuringHandler.Token mTabObscuringToken;

    /** The manager for overlay panels to attach listeners to. */
    private Supplier<OverlayPanelManager> mOverlayPanelManager;

    /** The last known activity tab, if available. */
    private Tab mLastActivityTab;

    /**
     * Used to track whether the active content has a custom scrim lifecycle. This is kept here
     * because there are some instances where the active content is changed prior to the close event
     * being called.
     */
    private boolean mContentHasCustomScrimLifecycle;

    /** The token used to enable browser controls persistence. */
    private int mPersistentControlsToken;

    /** A token used to suppress the bottom sheet in Tab switcher. */
    private int mTabSwitcherToken;

    public BottomSheetManager(ManagedBottomSheetController controller,
            ActivityTabProvider tabProvider,
            BrowserControlsVisibilityManager controlsVisibilityManager,
            Supplier<ModalDialogManager> dialogManager,
            Supplier<SnackbarManager> snackbarManagerSupplier,
            TabObscuringHandler obscuringDelegate,
            ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            Supplier<OverlayPanelManager> overlayManager,
            OneshotSupplier<StartSurface> startSurfaceSupplier,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            boolean isStartSurfaceRefactorEnabled) {
        mSheetController = controller;
        mTabProvider = tabProvider;
        mBrowserControlsVisibilityManager = controlsVisibilityManager;
        mDialogManager = dialogManager;
        mSnackbarManager = snackbarManagerSupplier;
        mTabObscuringHandler = obscuringDelegate;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
        mOverlayPanelManager = overlayManager;
        mCallbackController = new CallbackController();
        mIsStartSurfaceRefactorEnabled = isStartSurfaceRefactorEnabled;

        // TODO(https://crbug.com/1315679): Remove |mStartSurfaceSupplier|, |mStartSurfaceState| and
        // |mStartSurfaceStateObserver| after the refactor is enabled by default.
        mStartSurfaceSupplier = startSurfaceSupplier;
        if (!mIsStartSurfaceRefactorEnabled) {
            mStartSurfaceSupplier.onAvailable(
                    mCallbackController.makeCancelable(this::addStartSurfaceStateObserver));
        }

        mLayoutStateProviderSupplier = layoutStateProviderSupplier;
        mLayoutStateProviderSupplier.onAvailable(
                mCallbackController.makeCancelable(this::addLayoutStateObserver));

        mSheetController.addObserver(this);
        mSheetController.setAccessibilityUtil(ChromeAccessibilityUtil.get());

        // TODO(1092686): We should wait to instantiate all of these observers until the bottom
        //                sheet is actually used.
        mTabObserver = new EmptyTabObserver() {
            @Override
            public void onPageLoadStarted(Tab tab, GURL url) {
                controller.clearRequestsAndHide();
            }

            @Override
            public void onCrash(Tab tab) {
                controller.clearRequestsAndHide();
            }

            @Override
            public void onDestroyed(Tab tab) {
                if (mLastActivityTab != tab) return;
                mLastActivityTab = null;

                // Remove the suppressed sheet if its lifecycle is tied to the tab being destroyed.
                controller.clearRequestsAndHide();
            }
        };

        mTabProvider.addObserver(this::setActivityTab);
        setActivityTab(mTabProvider.get());

        mVrModeObserver = new VrModeObserver() {
            /** A token held while this object is suppressing the bottom sheet. */
            private int mToken;

            @Override
            public void onEnterVr() {
                mToken = controller.suppressSheet(StateChangeReason.VR);
            }

            @Override
            public void onExitVr() {
                controller.unsuppressSheet(mToken);
            }
        };
        VrModuleProvider.registerVrModeObserver(mVrModeObserver);

        mBrowserControlsObserver = new BrowserControlsVisibilityManager.Observer() {
            @Override
            public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
                    int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
                controller.setBrowserControlsHiddenRatio(
                        mBrowserControlsVisibilityManager.getBrowserControlHiddenRatio());
            }
        };
        mBrowserControlsVisibilityManager.addObserver(mBrowserControlsObserver);

        mOmniboxFocusObserver = new Callback<Boolean>() {
            /** A token held while this object is suppressing the bottom sheet. */
            private int mToken;

            @Override
            public void onResult(Boolean focused) {
                if (focused) {
                    mToken = controller.suppressSheet(BottomSheetController.StateChangeReason.NONE);
                } else {
                    controller.unsuppressSheet(mToken);
                }
            }
        };
        mOmniboxFocusStateSupplier.addObserver(mOmniboxFocusObserver);
    }

    private void setActivityTab(Tab tab) {
        // Temporarily suppress the sheet if entering a state where there is no activity
        // tab and the Start surface homepage isn't showing.
        updateSuppressionForTabSwitcher(tab,
                mStartSurfaceSupplier.get() == null
                        ? null
                        : mStartSurfaceSupplier.get().getStartSurfaceState(),
                mLayoutStateProviderSupplier.get() == null
                        ? null
                        : mLayoutStateProviderSupplier.get().getActiveLayoutType());

        if (tab == null) return;

        // If refocusing the same tab, simply unsuppress the sheet.
        if (mLastActivityTab == tab) return;

        // Move the observer to the new activity tab and clear the sheet.
        if (mLastActivityTab != null) mLastActivityTab.removeObserver(mTabObserver);
        mLastActivityTab = tab;
        mLastActivityTab.addObserver(mTabObserver);
        mSheetController.clearRequestsAndHide();
    }

    /**
     * Called by both {@link StateObserver} and {@link HintlessActivityTabObserver} to update the
     * suppression of the bottom sheet for Tab switcher.
     * @param tab The current tab. It might be null when the Start surface or the Tab switcher is
     *            showing.
     * @param startSurfaceState The current state surface state when the Start surface is enabled,
     *                          null otherwise. It's also null when the refactor is enabled.
     * @param layoutType The current layout type, currently only used when the refactor is enabled.
     */
    private void updateSuppressionForTabSwitcher(@Nullable Tab tab,
            @Nullable @StartSurfaceState Integer startSurfaceState,
            @Nullable @LayoutType Integer layoutType) {
        if (shouldSuppressForTabSwitcher(tab, startSurfaceState, layoutType)) {
            if (mTabSwitcherToken == 0) {
                mTabSwitcherToken = mSheetController.suppressSheet(StateChangeReason.COMPOSITED_UI);
            }
        } else {
            mSheetController.unsuppressSheet(mTabSwitcherToken);
            /**
             * Reset the token after unsuppression. Without resetting the token, the bottom sheet
             * won't be suppress again the next time entering Tab switcher. This is because the
             * bottom sheet is only suppressed in Tab switcher if {@link mTabSwitcherToken} is 0 by
             * the first observer who notices the event.
             */
            mTabSwitcherToken = 0;
        }
    }

    private boolean shouldSuppressForTabSwitcher(Tab tab,
            @StartSurfaceState Integer startSurfaceState,
            @Nullable @LayoutType Integer layoutType) {
        StartSurface startSurface = mStartSurfaceSupplier.get();

        if (mIsStartSurfaceRefactorEnabled) {
            if (layoutType == null) return tab == null;
            if (layoutType == LayoutType.START_SURFACE) {
                return false;
            } else if (layoutType == LayoutType.TAB_SWITCHER) {
                // If startSurface is not null,  start surface is enabled.
                return startSurface != null;
            }
        } else {
            if (startSurface == null || startSurfaceState == null) return tab == null;
            if (startSurfaceState == StartSurfaceState.SHOWING_HOMEPAGE
                    || startSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE) {
                return false;
            } else if (startSurfaceState != StartSurfaceState.NOT_SHOWN
                    && startSurfaceState != StartSurfaceState.DISABLED) {
                return true;
            }
        }
        return tab == null;
    }

    private void addStartSurfaceStateObserver(StartSurface startSurface) {
        mStartSurfaceStateObserver = new StateObserver() {
            private int mStartSurfaceState;
            @Override
            public void onStateChanged(
                    int startSurfaceState, boolean shouldShowTabSwitcherToolbar) {
                if (mStartSurfaceState == startSurfaceState) return;

                assert startSurfaceState == startSurface.getStartSurfaceState();
                mStartSurfaceState = startSurfaceState;
                updateSuppressionForTabSwitcher(mTabProvider.get(), startSurfaceState, null);

                if (startSurfaceState == StartSurfaceState.SHOWN_HOMEPAGE) {
                    mSheetController.clearRequestsAndHide();
                }
            }
        };
        startSurface.addStateChangeObserver(mStartSurfaceStateObserver);
    }

    private void addLayoutStateObserver(LayoutStateProvider layoutStateProvider) {
        if (!mIsStartSurfaceRefactorEnabled) return;

        mLayoutStateObserver = new LayoutStateObserver() {
            private @LayoutType int mLayoutType;
            @Override
            public void onFinishedShowing(int layoutType) {
                if (mLayoutType == layoutType) return;

                mLayoutType = layoutType;
                updateSuppressionForTabSwitcher(mTabProvider.get(), null, mLayoutType);
                if (mLayoutType == LayoutType.START_SURFACE) {
                    mSheetController.clearRequestsAndHide();
                }
            }
        };
        layoutStateProvider.addObserver(mLayoutStateObserver);
    }

    @Override
    public void onSheetOpened(int reason) {
        if (mBrowserControlsVisibilityManager.getBrowserVisibilityDelegate() != null) {
            // Browser controls should stay visible until the sheet is closed.
            mPersistentControlsToken =
                    mBrowserControlsVisibilityManager.getBrowserVisibilityDelegate()
                            .showControlsPersistent();
        }

        Tab activeTab = mTabProvider.get();
        if (activeTab != null) {
            WebContents webContents = activeTab.getWebContents();
            if (webContents != null) {
                SelectionPopupController.fromWebContents(webContents).clearSelection();
            }
        }

        if (mOverlayPanelManager.hasValue()
                && mOverlayPanelManager.get().getActivePanel() != null) {
            mOverlayPanelManager.get().getActivePanel().closePanel(
                    OverlayPanel.StateChangeReason.UNKNOWN, true);
        }

        BottomSheetContent content = mSheetController.getCurrentSheetContent();
        // Content with a custom scrim lifecycle should not obscure the tab. The feature
        // is responsible for adding itself to the list of obscuring views when applicable.
        if (content != null && content.hasCustomScrimLifecycle()) {
            mContentHasCustomScrimLifecycle = true;
            return;
        }

        setIsObscuringAllTabs(true);

        assert mAppModalToken == TokenHolder.INVALID_TOKEN;
        assert mTabModalToken == TokenHolder.INVALID_TOKEN;
        if (mDialogManager.get() != null) {
            mAppModalToken =
                    mDialogManager.get().suspendType(ModalDialogManager.ModalDialogType.APP);
            mTabModalToken =
                    mDialogManager.get().suspendType(ModalDialogManager.ModalDialogType.TAB);
        }
    }

    @Override
    public void onSheetClosed(int reason) {
        if (mBrowserControlsVisibilityManager.getBrowserVisibilityDelegate() != null) {
            // Update the browser controls since they are permanently shown while the sheet is
            // open.
            mBrowserControlsVisibilityManager.getBrowserVisibilityDelegate()
                    .releasePersistentShowingToken(mPersistentControlsToken);
        }

        BottomSheetContent content = mSheetController.getCurrentSheetContent();
        // If the content has a custom scrim, it wasn't obscuring tabs.
        if (mContentHasCustomScrimLifecycle) {
            mContentHasCustomScrimLifecycle = false;
            return;
        }

        setIsObscuringAllTabs(false);

        // Tokens can be invalid if the sheet has a custom lifecycle.
        if (mDialogManager.get() != null
                && (mAppModalToken != TokenHolder.INVALID_TOKEN
                        || mTabModalToken != TokenHolder.INVALID_TOKEN)) {
            // If one modal dialog token is set, the other should be as well.
            assert mAppModalToken != TokenHolder.INVALID_TOKEN
                    && mTabModalToken != TokenHolder.INVALID_TOKEN;
            mDialogManager.get().resumeType(ModalDialogManager.ModalDialogType.APP, mAppModalToken);
            mDialogManager.get().resumeType(ModalDialogManager.ModalDialogType.TAB, mTabModalToken);
        }
        mAppModalToken = TokenHolder.INVALID_TOKEN;
        mTabModalToken = TokenHolder.INVALID_TOKEN;
    }

    /**
     * Set whether the bottom sheet is obscuring all tabs.
     * @param isObscuring Whether the bottom sheet is considered to be obscuring.
     */
    private void setIsObscuringAllTabs(boolean isObscuring) {
        if (isObscuring) {
            assert mTabObscuringToken == null;
            mTabObscuringToken =
                    mTabObscuringHandler.obscure(TabObscuringHandler.Target.ALL_TABS_AND_TOOLBAR);
        } else {
            mTabObscuringHandler.unobscure(mTabObscuringToken);
            mTabObscuringToken = null;
        }
    }

    @Override
    public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
        if (mSnackbarManager.get() != null) {
            mSnackbarManager.get().dismissAllSnackbars();
        }
    }

    @Override
    public void onDestroy() {
        mCallbackController.destroy();
        if (mLastActivityTab != null) mLastActivityTab.removeObserver(mTabObserver);
        mSheetController.removeObserver(this);
        mBrowserControlsVisibilityManager.removeObserver(mBrowserControlsObserver);
        mOmniboxFocusStateSupplier.removeObserver(mOmniboxFocusObserver);
        VrModuleProvider.unregisterVrModeObserver(mVrModeObserver);
        if (mStartSurfaceSupplier.get() != null) {
            mStartSurfaceSupplier.get().removeStateChangeObserver(mStartSurfaceStateObserver);
        }
        if (mLayoutStateProviderSupplier.get() != null) {
            mLayoutStateProviderSupplier.get().removeObserver(mLayoutStateObserver);
        }
    }
}
