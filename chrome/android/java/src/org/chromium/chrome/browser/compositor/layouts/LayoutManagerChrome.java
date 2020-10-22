// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.accessibility_tab_switcher.OverviewListLayout;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.TitleCache;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.components.VirtualView;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EdgeSwipeHandler;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EmptyEdgeSwipeHandler;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.ScrollDirection;
import org.chromium.chrome.browser.compositor.layouts.phone.StackLayout;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManagementDelegate;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.List;

/**
 * A {@link Layout} controller for the more complicated Chrome browser.  This is currently a
 * superset of {@link LayoutManager}.
 */
public class LayoutManagerChrome
        extends LayoutManager implements OverviewModeController, ChromeAccessibilityUtil.Observer {
    // Layouts
    /** An {@link Layout} that should be used as the accessibility tab switcher. */
    protected OverviewListLayout mOverviewListLayout;
    /** A {@link Layout} that should be used when the user is swiping sideways on the toolbar. */
    protected ToolbarSwipeLayout mToolbarSwipeLayout;
    /** A {@link Layout} that should be used when the user is in the tab switcher. */
    protected Layout mOverviewLayout;

    // Event Filter Handlers
    private final EdgeSwipeHandler mToolbarSwipeHandler;

    // Internal State
    /** A {@link TitleCache} instance that stores all title/favicon bitmaps as CC resources. */
    protected TitleCache mTitleCache;

    /** Whether or not animations are enabled.  This can disable certain layouts or effects. */
    private boolean mEnableAnimations = true;
    private boolean mCreatingNtp;
    private final ObserverList<OverviewModeObserver> mOverviewModeObservers;

    /** Whether to create an overview Layout when LayoutManagerChrome is created. */
    private boolean mCreateOverviewLayout;

    protected ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    private final OneshotSupplierImpl<OverviewModeBehavior> mOverviewModeBehaviorSupplier;

    /**
     * Creates the {@link LayoutManagerChrome} instance.
     * @param host         A {@link LayoutManagerHost} instance.
     * @param contentContainer A {@link ViewGroup} for Android views to be bound to.
     * @param startSurface An interface to talk to the Grid Tab Switcher. If it's NULL, VTS
     *                     should be used, otherwise GTS should be used.
     * @param tabContentManagerSupplier Supplier of the {@link TabContentManager} instance.
     */
    public LayoutManagerChrome(LayoutManagerHost host, ViewGroup contentContainer,
            boolean createOverviewLayout, @Nullable StartSurface startSurface,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            OneshotSupplierImpl<OverviewModeBehavior> overviewModeBehaviorSupplier) {
        super(host, contentContainer, tabContentManagerSupplier);
        Context context = host.getContext();
        LayoutRenderHost renderHost = host.getLayoutRenderHost();

        mOverviewModeObservers = new ObserverList<OverviewModeObserver>();

        // Build Event Filter Handlers
        mToolbarSwipeHandler = createToolbarSwipeHandler(/* supportSwipeDown = */ true);

        mTabContentManagerSupplier = tabContentManagerSupplier;
        mTabContentManagerSupplier.addObserver(new Callback<TabContentManager>() {
            @Override
            public void onResult(TabContentManager manager) {
                if (mOverviewLayout != null) {
                    mOverviewLayout.setTabContentManager(manager);
                }
                tabContentManagerSupplier.removeObserver(this);
            }
        });

        if (createOverviewLayout) {
            if (startSurface != null) {
                assert TabUiFeatureUtilities.isGridTabSwitcherEnabled()
                        || StartSurfaceConfiguration.isStartSurfaceStackTabSwitcherEnabled();
                TabManagementDelegate tabManagementDelegate =
                        TabManagementModuleProvider.getDelegate();
                assert tabManagementDelegate != null;
                startSurface.setStateChangeObserver(new StartSurface.StateObserver() {
                    @Override
                    public void onStateChanged(@OverviewModeState int overviewModeState,
                            boolean shouldShowTabSwitcherToolbar) {
                        notifyObserversStateChanged(
                                overviewModeState, shouldShowTabSwitcherToolbar);
                    }
                });
                final ObservableSupplier<? extends BrowserControlsStateProvider>
                        browserControlsSupplier = mHost.getBrowserControlsManagerSupplier();
                mOverviewLayout = tabManagementDelegate.createStartSurfaceLayout(context, this,
                        renderHost, startSurface,
                        (ObservableSupplier<BrowserControlsStateProvider>) browserControlsSupplier);
            } else {
                mCreateOverviewLayout = true;
            }
        }

        mOverviewModeBehaviorSupplier = overviewModeBehaviorSupplier;
        mOverviewModeBehaviorSupplier.set(this);
    }

    /**
     * @return A list of virtual views representing compositor rendered views.
     */
    @Override
    public void getVirtualViews(List<VirtualView> views) {
        // TODO(dtrainor): Investigate order.
        for (int i = 0; i < mSceneOverlays.size(); i++) {
            if (!mSceneOverlays.get(i).isSceneOverlayTreeShowing()) continue;
            mSceneOverlays.get(i).getVirtualViews(views);
        }
    }

    /**
     * @return The {@link EdgeSwipeHandler} responsible for processing swipe events for the toolbar.
     */
    @Override
    public EdgeSwipeHandler getToolbarSwipeHandler() {
        return mToolbarSwipeHandler;
    }

    @Override
    public EdgeSwipeHandler createToolbarSwipeHandler(boolean supportSwipeDown) {
        return new ToolbarSwipeHandler(supportSwipeDown);
    }

    @Override
    public void init(TabModelSelector selector, TabCreatorManager creator,
            ControlContainer controlContainer,
            ContextualSearchManagementDelegate contextualSearchDelegate,
            DynamicResourceLoader dynamicResourceLoader, ActivityTabProvider tabProvider) {
        Context context = mHost.getContext();
        LayoutRenderHost renderHost = mHost.getLayoutRenderHost();
        BrowserControlsStateProvider browserControlsStateProvider =
                mHost.getBrowserControlsManager();

        // Build Layouts
        mOverviewListLayout =
                new OverviewListLayout(context, this, renderHost, browserControlsStateProvider);
        mToolbarSwipeLayout = new ToolbarSwipeLayout(context, this, renderHost);

        if (mCreateOverviewLayout) {
            final ObservableSupplier<? extends BrowserControlsStateProvider>
                    browserControlsSupplier = mHost.getBrowserControlsManagerSupplier();
            mOverviewLayout = new StackLayout(context, this, renderHost,
                    (ObservableSupplier<BrowserControlsStateProvider>) browserControlsSupplier);
        }

        super.init(selector, creator, controlContainer, contextualSearchDelegate,
                dynamicResourceLoader, tabProvider);

        // TODO: TitleCache should be a part of the ResourceManager.
        mTitleCache = mHost.getTitleCache();

        // Initialize Layouts
        TabContentManager content = mTabContentManagerSupplier.get();
        mToolbarSwipeLayout.setTabModelSelector(selector, content);
        mOverviewListLayout.setTabModelSelector(selector, content);
        if (mOverviewLayout != null) {
            mOverviewLayout.setTabModelSelector(selector, content);
            mOverviewLayout.onFinishNativeInitialization();
        }
    }

    @Override
    public void setTabModelSelector(TabModelSelector selector) {
        super.setTabModelSelector(selector);
        if (mOverviewLayout != null) {
            mOverviewLayout.setTabModelSelector(selector, null);
        }
    }

    @Override
    public void destroy() {
        super.destroy();
        mOverviewModeObservers.clear();

        if (mTabContentManagerSupplier != null) {
            mTabContentManagerSupplier = null;
        }

        if (mOverviewLayout != null) {
            mOverviewLayout.destroy();
            mOverviewLayout = null;
        }
        if (mOverviewListLayout != null) {
            mOverviewListLayout.destroy();
        }
        if (mToolbarSwipeLayout != null) {
            mToolbarSwipeLayout.destroy();
        }
    }

    /**
     * Simulates a click on the view at the specified pixel offset
     * from the top left of the view.
     * This is used by UI tests.
     * @param x Coordinate of the click in dp.
     * @param y Coordinate of the click in dp.
     */
    @VisibleForTesting
    public void simulateClick(float x, float y) {
        if (getActiveLayout() instanceof StackLayout) {
            ((StackLayout) getActiveLayout()).simulateClick(x, y);
        }
    }

    /**
     * Simulates a drag and issues Up-event to commit the drag.
     * @param x  Coordinate to start the Drag from in dp.
     * @param y  Coordinate to start the Drag from in dp.
     * @param dX Amount of drag in X direction in dp.
     * @param dY Amount of drag in Y direction in dp.
     */
    @VisibleForTesting
    public void simulateDrag(float x, float y, float dX, float dY) {
        if (getActiveLayout() instanceof StackLayout) {
            ((StackLayout) getActiveLayout()).simulateDrag(x, y, dX, dY);
        }
    }

    private boolean isOverviewLayout(Layout layout) {
        return layout != null && (layout == mOverviewLayout || layout == mOverviewListLayout);
    }

    @Override
    protected void startShowing(Layout layout, boolean animate) {
        mCreatingNtp = false;
        super.startShowing(layout, animate);

        Layout layoutBeingShown = getActiveLayout();

        // Check if a layout is showing that should hide the overlay panels.
        if (isOverviewLayout(layoutBeingShown) || layoutBeingShown == mToolbarSwipeLayout) {
            if (mContextualSearchDelegate != null) {
                mContextualSearchDelegate.dismissContextualSearchBar();
            }
        }

        // Check if we should notify OverviewModeObservers.
        if (isOverviewLayout(layoutBeingShown)) {
            boolean showToolbar = animate && (!mEnableAnimations
                    || getTabModelSelector().getCurrentModel().getCount() <= 0);
            notifyObserversStartedShowing(showToolbar);
        }
    }

    @Override
    public void startHiding(int nextTabId, boolean hintAtTabSelection) {
        super.startHiding(nextTabId, hintAtTabSelection);

        Layout layoutBeingHidden = getActiveLayout();
        if (isOverviewLayout(layoutBeingHidden)) {
            boolean showToolbar = true;
            if (mEnableAnimations && layoutBeingHidden == mOverviewLayout) {
                final LayoutTab tab = layoutBeingHidden.getLayoutTab(nextTabId);
                showToolbar = tab != null ? !tab.showToolbar() : true;
            }

            boolean creatingNtp = layoutBeingHidden == mOverviewLayout && mCreatingNtp;

            notifyObserversStartedHiding(showToolbar, creatingNtp);
        }
    }

    @Override
    public void doneShowing() {
        super.doneShowing();

        if (isOverviewLayout(getActiveLayout())) {
            notifyObserversFinishedShowing();
        }
    }

    @Override
    public void doneHiding() {
        Layout layoutBeingHidden = getActiveLayout();

        if (getNextLayout() == getDefaultLayout()) {
            Tab tab = getTabModelSelector() != null ? getTabModelSelector().getCurrentTab() : null;
            emptyCachesExcept(tab != null ? tab.getId() : Tab.INVALID_TAB_ID);
        }

        super.doneHiding();

        if (isOverviewLayout(layoutBeingHidden)) {
            notifyObserversFinishedHiding();
        }
    }

    @Override
    protected void tabCreated(int id, int sourceId, @TabLaunchType int launchType,
            boolean incognito, boolean willBeSelected, float originX, float originY) {
        Tab newTab = TabModelUtils.getTabById(getTabModelSelector().getModel(incognito), id);
        mCreatingNtp = newTab != null && newTab.isNativePage();
        super.tabCreated(id, sourceId, launchType, incognito, willBeSelected, originX, originY);
    }

    @Override
    public boolean closeAllTabsRequest(boolean incognito) {
        if (!isOverviewLayout(getActiveLayout())) return false;

        return super.closeAllTabsRequest(incognito);
    }

    @Override
    public void initLayoutTabFromHost(final int tabId) {
        if (mTitleCache != null) {
            mTitleCache.remove(tabId);
        }
        super.initLayoutTabFromHost(tabId);
    }

    @Override
    public void releaseResourcesForTab(int tabId) {
        super.releaseResourcesForTab(tabId);
        mTitleCache.remove(tabId);
    }

    /**
     * @return The {@link OverviewListLayout} managed by this class.
     */
    @VisibleForTesting
    public Layout getOverviewListLayout() {
        return mOverviewListLayout;
    }

    /**
     * @return The overview layout {@link Layout} managed by this class.
     */
    @VisibleForTesting
    public Layout getOverviewLayout() {
        return mOverviewLayout;
    }

    /**
     * @return The {@link StripLayoutHelperManager} managed by this class.
     */
    @VisibleForTesting
    public StripLayoutHelperManager getStripLayoutHelperManager() {
        return null;
    }

    /**
     * Show the overview {@link Layout}.  This is generally a {@link Layout} that visibly represents
     * all of the {@link Tab}s opened by the user.
     * @param animate Whether or not to animate the transition to overview mode.
     */
    @Override
    public void showOverview(boolean animate) {
        boolean useAccessibility = DeviceClassManager.enableAccessibilityLayout();

        boolean accessibilityIsVisible =
                useAccessibility && getActiveLayout() == mOverviewListLayout;
        boolean normalIsVisible = getActiveLayout() == mOverviewLayout && mOverviewLayout != null;

        // We only want to use the AccessibilityOverviewLayout if the following are all valid:
        // 1. We're already showing the AccessibilityOverviewLayout OR we're using accessibility.
        // 2. We're not already showing the normal OverviewLayout (or we are on a tablet, in which
        //    case the normal layout is always visible).
        if ((accessibilityIsVisible || useAccessibility) && !normalIsVisible) {
            startShowing(mOverviewListLayout, animate);
        } else if (mOverviewLayout != null) {
            startShowing(mOverviewLayout, animate);
        }
    }

    /**
     * Hides the current {@link Layout}, returning to the default {@link Layout}.
     * @param animate Whether or not to animate the transition to the default {@link Layout}.
     */
    @Override
    public void hideOverview(boolean animate) {
        Layout activeLayout = getActiveLayout();
        if (activeLayout != null && !activeLayout.isHiding()) {
            if (animate) {
                activeLayout.onTabSelecting(time(), Tab.INVALID_TAB_ID);
            } else {
                startHiding(Tab.INVALID_TAB_ID, false);
                doneHiding();
            }
        }
    }

    /**
     * @param enabled Whether or not to allow model-reactive animations (tab creation, closing,
     *                etc.).
     */
    public void setEnableAnimations(boolean enabled) {
        mEnableAnimations = enabled;
    }

    /**
     * @return Whether animations should be done for model changes.
     */
    @VisibleForTesting
    public boolean animationsEnabled() {
        return mEnableAnimations;
    }

    @Override
    public boolean overviewVisible() {
        Layout activeLayout = getActiveLayout();
        return isOverviewLayout(activeLayout) && !activeLayout.isHiding();
    }

    @Override
    public void addOverviewModeObserver(OverviewModeObserver listener) {
        mOverviewModeObservers.addObserver(listener);
    }

    @Override
    public void removeOverviewModeObserver(OverviewModeObserver listener) {
        mOverviewModeObservers.removeObserver(listener);
    }

    // ChromeAccessibilityUtil.Observer

    @Override
    public void onAccessibilityModeChanged(boolean enabled) {
        setEnableAnimations(DeviceClassManager.enableAnimations());
    }

    /**
     * A {@link EdgeSwipeHandler} meant to respond to edge events for the toolbar.
     */
    protected class ToolbarSwipeHandler extends EmptyEdgeSwipeHandler {
        /** The scroll direction of the current gesture. */
        private @ScrollDirection int mScrollDirection;

        /**
         * The range in degrees that a swipe can be from a particular direction to be considered
         * that direction.
         */
        private static final float SWIPE_RANGE_DEG = 25;

        private final boolean mSupportSwipeDown;

        public ToolbarSwipeHandler(boolean supportSwipeDown) {
            mSupportSwipeDown = supportSwipeDown;
        }

        @Override
        public void swipeStarted(@ScrollDirection int direction, float x, float y) {
            mScrollDirection = ScrollDirection.UNKNOWN;
        }

        @Override
        public void swipeUpdated(float x, float y, float dx, float dy, float tx, float ty) {
            if (mToolbarSwipeLayout == null) return;

            // If scroll direction has been computed, send the event to super.
            if (mScrollDirection != ScrollDirection.UNKNOWN) {
                mToolbarSwipeLayout.swipeUpdated(time(), x, y, dx, dy, tx, ty);
                return;
            }

            mScrollDirection = computeScrollDirection(dx, dy);
            if (mScrollDirection == ScrollDirection.UNKNOWN) return;

            if (mSupportSwipeDown && mOverviewLayout != null
                    && mScrollDirection == ScrollDirection.DOWN) {
                RecordUserAction.record("MobileToolbarSwipeOpenStackView");
                showOverview(true);
            } else if (mScrollDirection == ScrollDirection.LEFT
                    || mScrollDirection == ScrollDirection.RIGHT) {
                startShowing(mToolbarSwipeLayout, true);
            }

            mToolbarSwipeLayout.swipeStarted(time(), mScrollDirection, x, y);
        }

        @Override
        public void swipeFinished() {
            if (mToolbarSwipeLayout == null || !mToolbarSwipeLayout.isActive()) return;
            mToolbarSwipeLayout.swipeFinished(time());
        }

        @Override
        public void swipeFlingOccurred(float x, float y, float tx, float ty, float vx, float vy) {
            if (mToolbarSwipeLayout == null || !mToolbarSwipeLayout.isActive()) return;
            mToolbarSwipeLayout.swipeFlingOccurred(time(), x, y, tx, ty, vx, vy);
        }

        /**
         * Compute the direction of the scroll.
         * @param dx The distance traveled on the X axis.
         * @param dy The distance traveled on the Y axis.
         * @return The direction of the scroll.
         */
        private @ScrollDirection int computeScrollDirection(float dx, float dy) {
            @ScrollDirection
            int direction = ScrollDirection.UNKNOWN;

            // Figure out the angle of the swipe. Invert 'dy' so 90 degrees is up.
            double swipeAngle = (Math.toDegrees(Math.atan2(-dy, dx)) + 360) % 360;

            if (swipeAngle < 180 + SWIPE_RANGE_DEG && swipeAngle > 180 - SWIPE_RANGE_DEG) {
                direction = ScrollDirection.LEFT;
            } else if (swipeAngle < SWIPE_RANGE_DEG || swipeAngle > 360 - SWIPE_RANGE_DEG) {
                direction = ScrollDirection.RIGHT;
            } else if (swipeAngle < 270 + SWIPE_RANGE_DEG && swipeAngle > 270 - SWIPE_RANGE_DEG) {
                direction = ScrollDirection.DOWN;
            }

            return direction;
        }

        @Override
        public boolean isSwipeEnabled(@ScrollDirection int direction) {
            FullscreenManager manager = mHost.getFullscreenManager();
            if (getActiveLayout() != mStaticLayout
                    || !DeviceClassManager.enableToolbarSwipe()
                    || (manager != null && manager.getPersistentFullscreenMode())) {
                return false;
            }

            if (direction == ScrollDirection.DOWN) {
                boolean isAccessibility = ChromeAccessibilityUtil.get().isAccessibilityEnabled();
                return mOverviewLayout != null && !isAccessibility;
            }

            return direction == ScrollDirection.LEFT || direction == ScrollDirection.RIGHT;
        }
    }

    /**
     * @param id The id of the {@link Tab} to search for.
     * @return   A {@link Tab} instance or {@code null} if it could be found.
     */
    protected Tab getTabById(int id) {
        TabModelSelector selector = getTabModelSelector();
        return selector == null ? null : selector.getTabById(id);
    }

    @Override
    protected void switchToTab(Tab tab, int lastTabId) {
        if (tab == null || lastTabId == Tab.INVALID_TAB_ID) {
            super.switchToTab(tab, lastTabId);
            return;
        }
        startShowing(mToolbarSwipeLayout, false);
        mToolbarSwipeLayout.switchToTab(tab.getId(), lastTabId);

        // Close the previous tab if the previous tab is a NTP.
        Tab lastTab = getTabById(lastTabId);
        if (NewTabPage.isNTPUrl(lastTab.getUrl()) && !lastTab.canGoBack()
                && !lastTab.canGoForward()) {
            getTabModelSelector()
                    .getModel(lastTab.isIncognito())
                    .closeTab(lastTab, tab, false, false, false);
        }
    }

    private void notifyObserversStartedShowing(boolean showToolbar) {
        mOverviewModeBehaviorSupplier.onAvailable((unused) -> {
            for (OverviewModeObserver overviewModeObserver : mOverviewModeObservers) {
                overviewModeObserver.onOverviewModeStartedShowing(showToolbar);
            }
        });
    }

    private void notifyObserversFinishedShowing() {
        mOverviewModeBehaviorSupplier.onAvailable((unused) -> {
            for (OverviewModeObserver overviewModeObserver : mOverviewModeObservers) {
                overviewModeObserver.onOverviewModeFinishedShowing();
            }
        });
    }

    private void notifyObserversStartedHiding(boolean showToolbar, boolean creatingNtp) {
        mOverviewModeBehaviorSupplier.onAvailable((unused) -> {
            for (OverviewModeObserver overviewModeObserver : mOverviewModeObservers) {
                overviewModeObserver.onOverviewModeStartedHiding(showToolbar, creatingNtp);
            }
        });
    }

    private void notifyObserversFinishedHiding() {
        mOverviewModeBehaviorSupplier.onAvailable((unused) -> {
            for (OverviewModeObserver overviewModeObserver : mOverviewModeObservers) {
                overviewModeObserver.onOverviewModeFinishedHiding();
            }
        });
    }

    private void notifyObserversStateChanged(
            @OverviewModeState int overviewModeState, boolean shouldShowTabSwitcherToolbar) {
        mOverviewModeBehaviorSupplier.onAvailable((unused) -> {
            for (OverviewModeObserver overviewModeObserver : mOverviewModeObservers) {
                overviewModeObserver.onOverviewModeStateChanged(
                        overviewModeState, shouldShowTabSwitcherToolbar);
            }
        });
    }
}
