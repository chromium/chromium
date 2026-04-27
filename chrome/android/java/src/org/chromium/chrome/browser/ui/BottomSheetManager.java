// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BottomControlsLayer;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerScrollBehavior;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerVisibility;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.compositor.overlay_panel.OverlayPanel;
import org.chromium.chrome.browser.compositor.overlay_panel.OverlayPanelManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.ExpandedSheetHelper;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.function.Supplier;

/**
 * A class that manages activity-specific interactions with the BottomSheet component that it
 * otherwise shouldn't know about.
 */
@NullMarked
class BottomSheetManager extends EmptyBottomSheetObserver implements DestroyObserver {
    /** A means of accessing the focus state of the omnibox. */
    private final MonotonicObservableSupplier<Boolean> mOmniboxFocusStateSupplier;

    /** An observer of the omnibox that suppresses the sheet when the omnibox is focused. */
    private final Callback<Boolean> mOmniboxFocusObserver;

    /** A listener for browser controls offset changes. */
    private final BrowserControlsStateProvider.Observer mBrowserControlsObserver;

    private final BottomSheetLayer mBottomSheetLayer;

    /** A tab observer that is only attached to the active tab. */
    private final TabObserver mTabObserver;

    private final CallbackController mCallbackController;

    private final OneshotSupplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private final LayoutStateProvider.LayoutStateObserver mLayoutStateObserver;

    private final ExpandedSheetHelper mExpandedSheetHelper;

    /** A browser controls manager for polling browser controls offsets. */
    private final BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;

    /**
     * A handle to the {@link ManagedBottomSheetController} this class manages interactions with.
     */
    private final ManagedBottomSheetController mSheetController;

    private final BottomControlsStacker mBottomControlsStacker;

    /** A mechanism for accessing the currently active tab. */
    private final ActivityTabProvider mTabProvider;

    /** The manager for overlay panels to attach listeners to. */
    private final Supplier<@Nullable OverlayPanelManager> mOverlayPanelManager;

    private final Callback<@Nullable Tab> mOnActiveTabChanged = this::setActivityTab;

    /** The last known activity tab, if available. */
    private @Nullable Tab mLastActivityTab;

    /**
     * Used to track whether the active content has a custom scrim lifecycle. This is kept here
     * because there are some instances where the active content is changed prior to the close event
     * being called.
     */
    private boolean mContentHasCustomScrimLifecycle;

    /** The token used to enable browser controls persistence. */
    private int mPersistentControlsToken;

    /**
     * Creates a new instance of {@link BottomSheetManager}.
     *
     * @param controller The {@link ManagedBottomSheetController} to interact with.
     * @param tabProvider The {@link ActivityTabProvider} to listen to tab changes.
     * @param controlsVisibilityManager The {@link BrowserControlsVisibilityManager} to interact
     *     with browser controls.
     * @param expandedSheetHelper The {@link ExpandedSheetHelper} to notify about sheet expansion.
     * @param omniboxFocusStateSupplier The supplier for omnibox focus state.
     * @param overlayManager The supplier for {@link OverlayPanelManager}.
     * @param layoutStateProviderSupplier The supplier for {@link LayoutStateProvider}.
     * @param bottomControlsStacker The {@link BottomControlsStacker} to register this layer with.
     */
    public BottomSheetManager(
            ManagedBottomSheetController controller,
            ActivityTabProvider tabProvider,
            BrowserControlsVisibilityManager controlsVisibilityManager,
            ExpandedSheetHelper expandedSheetHelper,
            MonotonicObservableSupplier<Boolean> omniboxFocusStateSupplier,
            Supplier<@Nullable OverlayPanelManager> overlayManager,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            BottomControlsStacker bottomControlsStacker) {
        mSheetController = controller;
        mTabProvider = tabProvider;
        mBrowserControlsVisibilityManager = controlsVisibilityManager;
        mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
        mOverlayPanelManager = overlayManager;
        mCallbackController = new CallbackController();
        mExpandedSheetHelper = expandedSheetHelper;
        mBottomControlsStacker = bottomControlsStacker;

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

        mLayoutStateProviderSupplier = layoutStateProviderSupplier;
        mLayoutStateProviderSupplier.onAvailable(
                mCallbackController.makeCancelable(
                        (provider) -> provider.addObserver(mLayoutStateObserver)));

        mSheetController.addObserver(this);

        // TODO(crbug.com/40134698): We should wait to instantiate all of these observers until the
        // bottom sheet is actually used.
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

        mTabProvider.asObservable().addSyncObserverAndCallIfNonNull(mOnActiveTabChanged);

        mBrowserControlsObserver =
                new BrowserControlsStateProvider.Observer() {
                    @Override
                    public void onControlsOffsetChanged(
                            int topOffset,
                            int topControlsMinHeightOffset,
                            boolean topControlsMinHeightChanged,
                            int bottomOffset,
                            int bottomControlsMinHeightOffset,
                            boolean bottomControlsMinHeightChanged,
                            boolean requestNewFrame,
                            boolean isVisibilityForced) {
                        controller.setBrowserControlsHiddenRatio(
                                mBrowserControlsVisibilityManager.getBrowserControlHiddenRatio());
                    }

                    @Override
                    public void onBottomControlsHeightChanged(
                            int bottomControlsHeight, int bottomControlsMinHeight) {
                        mSheetController.setBottomControlsOffset(bottomControlsHeight);
                    }
                };

        mBottomSheetLayer = new BottomSheetLayer();
        if (ChromeFeatureList.sBottomSheetAsBrowserControls.isEnabled()) {
            mSheetController.addObserver(mBottomSheetLayer);
            mBottomControlsStacker.addLayer(mBottomSheetLayer);
        } else {
            mBrowserControlsVisibilityManager.addObserver(mBrowserControlsObserver);
        }

        mOmniboxFocusObserver =
                new Callback<>() {
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
        mOmniboxFocusStateSupplier.addSyncObserverAndPostIfNonNull(mOmniboxFocusObserver);
        if (ChromeFeatureList.sBottomSheetAsBrowserControls.isEnabled()) {
            mBottomSheetLayer.maybeUpdateLayerHeight();
        }
    }

    /** Returns the {@link BottomControlsLayer} for the bottom sheet. */
    public BottomControlsLayer getBottomSheetControlsLayer() {
        return mBottomSheetLayer;
    }

    private void setActivityTab(@Nullable Tab tab) {
        if (tab == null) return;
        if (mLastActivityTab == tab) return;

        // Move the observer to the new activity tab and clear the sheet.
        if (mLastActivityTab != null) mLastActivityTab.removeObserver(mTabObserver);
        mLastActivityTab = tab;
        mLastActivityTab.addObserver(mTabObserver);
        mSheetController.clearRequestsAndHide();
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

        OverlayPanelManager overlayPanelManager = mOverlayPanelManager.get();
        if (overlayPanelManager != null) {
            OverlayPanel activePanel = overlayPanelManager.getActivePanel();
            if (activePanel != null) {
                activePanel.closePanel(OverlayPanel.StateChangeReason.UNKNOWN, true);
            }
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

        // If the content has a custom scrim, it wasn't obscuring tabs.
        if (mContentHasCustomScrimLifecycle) {
            mContentHasCustomScrimLifecycle = false;
            return;
        }

        mExpandedSheetHelper.onSheetCollapsed();
    }

    @Override
    public void onDestroy() {
        if (ChromeFeatureList.sBottomSheetAsBrowserControls.isEnabled()) {
            mBottomControlsStacker.removeLayer(mBottomSheetLayer);
            mSheetController.removeObserver(mBottomSheetLayer);
        } else {
            mBrowserControlsVisibilityManager.removeObserver(mBrowserControlsObserver);
        }
        mCallbackController.destroy();
        if (mLastActivityTab != null) mLastActivityTab.removeObserver(mTabObserver);
        mSheetController.removeObserver(this);
        mTabProvider.asObservable().removeObserver(mOnActiveTabChanged);

        mOmniboxFocusStateSupplier.removeObserver(mOmniboxFocusObserver);
        var layoutStateProvider = mLayoutStateProviderSupplier.get();
        if (layoutStateProvider != null) {
            layoutStateProvider.removeObserver(mLayoutStateObserver);
        }
    }

    // Bottom controls layer that represents bottom sheet.
    // When bottom sheet is used as controls, it will contribute the browser controls by the height
    // of bottom sheet peek mode height;
    // when bottom sheet is not used as controls, the layer will attach on top of the browser
    // controls, making the controls non-scrollable.
    private class BottomSheetLayer extends EmptyBottomSheetObserver implements BottomControlsLayer {
        private int mContributedHeight;
        private int mContributedVisibility;

        @Override
        public void onSheetOpened(int reason) {
            maybeUpdateLayerHeight();
        }

        @Override
        public void onSheetStateChanged(int newState, int reason) {
            maybeUpdateLayerHeight();
        }

        @Override
        public void onSheetContentChanged(@Nullable BottomSheetContent newContent) {
            maybeUpdateLayerHeight();
        }

        @Override
        public void onContainerSizeChanged(int newWidth, int newHeight) {
            maybeUpdateLayerHeight();
        }

        // BottomControlsLayer

        @Override
        public int getType() {
            return BottomControlsStacker.LayerType.BOTTOM_SHEET;
        }

        @Override
        public int getScrollBehavior() {
            // When bottom sheet presents, we stop the rest of the browser controls from
            // scrolling, regardless whether the content want to actAsBrowserControls.
            // This is to avoid browser controls scroll off while the sheet is in PEEK mode,
            // leaving a piece of blank on screen.
            // In order to scroll the bottom sheet down with browser controls together is feasible,
            // however it will never be in sync with the rest of the compositor views, so we are
            // disabling this functionality.
            return LayerScrollBehavior.NEVER_SCROLL_OFF;
        }

        @Override
        public int getHeight() {
            return mContributedHeight;
        }

        @Override
        public int getLayerVisibility() {
            return mSheetController.getSheetState() == BottomSheetController.SheetState.HIDDEN
                    ? LayerVisibility.HIDDEN
                    : LayerVisibility.VISIBLE;
        }

        @Override
        public @Nullable Integer getBackgroundColor() {
            return BottomSheetUtils.isContentActingAsBrowserControls(mSheetController)
                    ? mSheetController.getSheetBackgroundColor()
                    : null;
        }

        @Override
        public void onBrowserControlsOffsetUpdate(int layerYOffset) {
            // The layerYOffset is the distance between the bottom of the layer and the bottom
            // of the screen, so it does not include the bottom sheet's height. The value of
            // `-layerYOffset` should equal to the visible height of the browser controls on screen.
            // Early return is handled by `mSheetController` so dispatching same yOffset
            // will not trigger addition layouts.
            int offset = Math.max(-layerYOffset, 0);
            mSheetController.setBottomControlsOffset(offset);

            mSheetController.setBrowserControlsHiddenRatio(
                    mBrowserControlsVisibilityManager.getBrowserControlHiddenRatio());
        }

        private void maybeUpdateLayerHeight() {
            int currentHeight = calculateContributedHeight();
            int currentVisibility = getLayerVisibility();
            if (currentHeight != mContributedHeight
                    || currentVisibility != mContributedVisibility) {
                mContributedHeight = currentHeight;
                mContributedVisibility = currentVisibility;
                mBottomControlsStacker.requestLayerUpdate(false);
            }
        }

        private int calculateContributedHeight() {
            if (!ChromeFeatureList.sBottomSheetAsBrowserControls.isEnabled()) {
                return 0;
            }
            if (mSheetController.getSheetState() == BottomSheetController.SheetState.HIDDEN
                    || mSheetController.isSheetHiding()) {
                return 0;
            }
            if (!mSheetController.isFullWidth()) {
                return 0;
            }
            return BottomSheetUtils.isContentActingAsBrowserControls(mSheetController)
                    ? mSheetController.getCurrentPeekHeightPx()
                    : 0;
        }
    }
}
