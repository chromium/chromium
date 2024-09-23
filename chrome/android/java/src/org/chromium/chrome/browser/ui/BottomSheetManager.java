// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

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
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.ExpandedSheetHelper;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
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

    /** A listener for browser controls offset changes. */
    private final BrowserControlsVisibilityManager.Observer mBrowserControlsObserver;

    /** A tab observer that is only attached to the active tab. */
    private final TabObserver mTabObserver;

    private final CallbackController mCallbackController;

    private final OneshotSupplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private LayoutStateProvider.LayoutStateObserver mLayoutStateObserver;

    private final ExpandedSheetHelper mExpandedSheetHelper;

    /** A browser controls manager for polling browser controls offsets. */
    private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;

    /**
     * A handle to the {@link ManagedBottomSheetController} this class manages interactions with.
     */
    private ManagedBottomSheetController mSheetController;

    /** A mechanism for accessing the currently active tab. */
    private ActivityTabProvider mTabProvider;

    /** A supplier of a snackbar manager for the bottom sheet. */
    private Supplier<SnackbarManager> mSnackbarManager;

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

    public BottomSheetManager(
            ManagedBottomSheetController controller,
            ActivityTabProvider tabProvider,
            BrowserControlsVisibilityManager controlsVisibilityManager,
            ExpandedSheetHelper expandedSheetHelper,
            Supplier<SnackbarManager> snackbarManagerSupplier,
            ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            Supplier<OverlayPanelManager> overlayManager,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier) {
        mSheetController = controller;
        mTabProvider = tabProvider;
        mBrowserControlsVisibilityManager = controlsVisibilityManager;
        mSnackbarManager = snackbarManagerSupplier;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
        mOverlayPanelManager = overlayManager;
        mCallbackController = new CallbackController();
        mExpandedSheetHelper = expandedSheetHelper;

        mLayoutStateProviderSupplier = layoutStateProviderSupplier;
        mLayoutStateProviderSupplier.onAvailable(
                mCallbackController.makeCancelable(this::addLayoutStateObserver));

        mSheetController.addObserver(this);

        // TODO(crbug.com/40134698): We should wait to instantiate all of these observers until the
        // bottom
        //                sheet is actually used.
        mTabObserver =
                new EmptyTabObserver() {
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

                        // Remove the suppressed sheet if its lifecycle is tied to the tab being
                        // destroyed.
                        controller.clearRequestsAndHide();
                    }
                };

        mTabProvider.addObserver(this::setActivityTab);
        setActivityTab(mTabProvider.get());

        mBrowserControlsObserver =
                new BrowserControlsVisibilityManager.Observer() {
                    @Override
                    public void onControlsOffsetChanged(
                            int topOffset,
                            int topControlsMinHeightOffset,
                            int bottomOffset,
                            int bottomControlsMinHeightOffset,
                            boolean needsAnimate,
                            boolean isVisibilityForced) {
                        controller.setBrowserControlsHiddenRatio(
                                mBrowserControlsVisibilityManager.getBrowserControlHiddenRatio());
                    }
                };
        mBrowserControlsVisibilityManager.addObserver(mBrowserControlsObserver);

        mOmniboxFocusObserver =
                new Callback<Boolean>() {
                    /** A token held while this object is suppressing the bottom sheet. */
                    private int mToken;

                    @Override
                    public void onResult(Boolean focused) {
                        if (focused) {
                            mToken =
                                    controller.suppressSheet(
                                            BottomSheetController.StateChangeReason.NONE);
                        } else {
                            controller.unsuppressSheet(mToken);
                        }
                    }
                };
        mOmniboxFocusStateSupplier.addObserver(mOmniboxFocusObserver);
    }

    private void setActivityTab(Tab tab) {
        if (tab == null) return;

        if (mLastActivityTab == tab) return;

        // Move the observer to the new activity tab and clear the sheet.
        if (mLastActivityTab != null) mLastActivityTab.removeObserver(mTabObserver);
        mLastActivityTab = tab;
        mLastActivityTab.addObserver(mTabObserver);
        mSheetController.clearRequestsAndHide();
    }

    private void addLayoutStateObserver(LayoutStateProvider layoutStateProvider) {
        mLayoutStateObserver =
                new LayoutStateObserver() {
                    // On switching to a new layout act as though this is a tab switch by clearing
                    // all state. Use onStartedHiding to avoid the bottom sheet being visible
                    // during the transition if there is one.
                    @Override
                    public void onStartedHiding(int layoutType) {
                        if (layoutType != LayoutType.SIMPLE_ANIMATION) {
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
                    mBrowserControlsVisibilityManager
                            .getBrowserVisibilityDelegate()
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
            mOverlayPanelManager
                    .get()
                    .getActivePanel()
                    .closePanel(OverlayPanel.StateChangeReason.UNKNOWN, true);
        }

        BottomSheetContent content = mSheetController.getCurrentSheetContent();
        // Content with a custom scrim lifecycle should not obscure the tab. The feature
        // is responsible for adding itself to the list of obscuring views when applicable.
        if (content != null && content.hasCustomScrimLifecycle()) {
            mContentHasCustomScrimLifecycle = true;
            return;
        }

        mExpandedSheetHelper.onSheetExpanded();
    }

    @Override
    public void onSheetClosed(int reason) {
        if (mBrowserControlsVisibilityManager.getBrowserVisibilityDelegate() != null) {
            // Update the browser controls since they are permanently shown while the sheet is
            // open.
            mBrowserControlsVisibilityManager
                    .getBrowserVisibilityDelegate()
                    .releasePersistentShowingToken(mPersistentControlsToken);
        }

        BottomSheetContent content = mSheetController.getCurrentSheetContent();
        // If the content has a custom scrim, it wasn't obscuring tabs.
        if (mContentHasCustomScrimLifecycle) {
            mContentHasCustomScrimLifecycle = false;
            return;
        }

        mExpandedSheetHelper.onSheetCollapsed();
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
        if (mLayoutStateProviderSupplier.get() != null) {
            mLayoutStateProviderSupplier.get().removeObserver(mLayoutStateObserver);
        }
    }
}
