// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.view.MotionEvent;
import android.view.ViewGroup;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.List;

/**
 * A {@link Layout} controller for the more complicated Chrome browser.  This is currently a
 * superset of {@link LayoutManagerImpl}.
 */
public class LayoutManagerChrome extends LayoutManagerImpl
        implements OverviewModeBehavior, ChromeAccessibilityUtil.Observer {
    // Layouts

    // Event Filter Handlers
    private final SwipeHandler mToolbarSwipeHandler;

    /** Whether or not animations are enabled.  This can disable certain layouts or effects. */
    private boolean mEnableAnimations = true;
    private boolean mCreatingNtp;
    private final ObserverList<OverviewModeObserver> mOverviewModeObservers;
    private LayoutStateObserver mTabSwitcherFocusLayoutStateObserver;

    protected ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    private final OneshotSupplierImpl<OverviewModeBehavior> mOverviewModeBehaviorSupplier;

    /**
     * Creates the {@link LayoutManagerChrome} instance.
     * @param host         A {@link LayoutManagerHost} instance.
     * @param contentContainer A {@link ViewGroup} for Android views to be bound to.
     * @param createOverviewLayout Whether overview layout should be created or not.
     * @param startSurface An interface to talk to the Grid Tab Switcher. If it's NULL, VTS
     *                     should be used, otherwise GTS should be used.
     * @param tabContentManagerSupplier Supplier of the {@link TabContentManager} instance.
     * @param overviewModeBehaviorSupplier Supplier of the {@link OverviewModeBehavior}.
     * @param topUiThemeColorProvider {@link ThemeColorProvider} for top UI.
     * @param scrimCoordinator {@link ScrimCoordinator} to show/hide scrim.
     */
    public LayoutManagerChrome(LayoutManagerHost host, ViewGroup contentContainer,
            boolean createOverviewLayout, @Nullable StartSurface startSurface,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            OneshotSupplierImpl<OverviewModeBehavior> overviewModeBehaviorSupplier,
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider, JankTracker jankTracker,
            ScrimCoordinator scrimCoordinator) {
        super(host, contentContainer, tabContentManagerSupplier, topUiThemeColorProvider);
        Context context = host.getContext();
        LayoutRenderHost renderHost = host.getLayoutRenderHost();

        mOverviewModeObservers = new ObserverList<OverviewModeObserver>();

        // Build Event Filter Handlers
        mToolbarSwipeHandler = createToolbarSwipeHandler(/* supportSwipeDown = */ true);

        mTabContentManagerSupplier = tabContentManagerSupplier;
        mTabContentManagerSupplier.addObserver(new Callback<TabContentManager>() {
            @Override
            public void onResult(TabContentManager manager) {
                manager.addThumbnailChangeListener((id) -> requestUpdate());
                tabContentManagerSupplier.removeObserver(this);
            }
        });

        if (createOverviewLayout) {
            if (startSurface != null) {
                assert TabUiFeatureUtilities.isGridTabSwitcherEnabled(context);
                TabManagementDelegate tabManagementDelegate =
                        TabManagementModuleProvider.getDelegate();
                assert tabManagementDelegate != null;

                if (TabUiFeatureUtilities.isTabletGridTabSwitcherEnabled(context)) {
                    mTabSwitcherFocusLayoutStateObserver = new LayoutStateObserver() {
                        @Override
                        public void onFinishedShowing(int layoutType) {
                            if (layoutType == LayoutType.TAB_SWITCHER) {
                                startSurface.getGridTabListDelegate().requestFocusOnCurrentTab();
                            }
                        }
                    };
                    addObserver(mTabSwitcherFocusLayoutStateObserver);
                }
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
     * @return The {@link SwipeHandler} responsible for processing swipe events for the toolbar.
     */
    @Override
    public SwipeHandler getToolbarSwipeHandler() {
        return mToolbarSwipeHandler;
    }

    @Override
    public SwipeHandler createToolbarSwipeHandler(boolean supportSwipeDown) {
        return new ToolbarSwipeHandler(supportSwipeDown);
    }

    @Override
    public void init(TabModelSelector selector, TabCreatorManager creator,
            DynamicResourceLoader dynamicResourceLoader,
            TopUiThemeColorProvider topUiColorProvider) {
        Context context = mHost.getContext();
        LayoutRenderHost renderHost = mHost.getLayoutRenderHost();
        BrowserControlsStateProvider browserControlsStateProvider =
                mHost.getBrowserControlsManager();

        super.init(selector, creator, dynamicResourceLoader, topUiColorProvider);

        // Initialize Layouts
        TabContentManager content = mTabContentManagerSupplier.get();
    }

    @Override
    public void setTabModelSelector(TabModelSelector selector) {
        super.setTabModelSelector(selector);
    }

    @Override
    public void destroy() {
        super.destroy();
        mOverviewModeObservers.clear();

        if (mTabContentManagerSupplier != null) {
            mTabContentManagerSupplier = null;
        }

        if (mTabSwitcherFocusLayoutStateObserver != null) {
            removeObserver(mTabSwitcherFocusLayoutStateObserver);
            mTabSwitcherFocusLayoutStateObserver = null;
        }
    }

    private boolean isOverviewLayout(Layout layout) {
        return false;
    }

    @Override
    protected Layout getLayoutForType(@LayoutType int layoutType) {
        return super.getLayoutForType(layoutType);
    }

    @Override
    protected void startShowing(Layout layout, boolean animate) {
        mCreatingNtp = false;
        super.startShowing(layout, animate);

        Layout layoutBeingShown = getActiveLayout();

        // TODO(crbug.com/1108496): Remove after migrates to LayoutStateObserver.
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

        // TODO(crbug.com/1108496): Remove after migrates to LayoutStateObserver.
        Layout layoutBeingHidden = getActiveLayout();
        if (isOverviewLayout(layoutBeingHidden)) {
            boolean showToolbar = true;

            notifyObserversStartedHiding(showToolbar, false);
        }
    }

    @Override
    public void doneShowing() {
        super.doneShowing();

        // TODO(crbug.com/1108496): Remove after migrates to LayoutStateObserver.
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

        // TODO(crbug.com/1108496): Remove after migrates to Observer.
        if (isOverviewLayout(layoutBeingHidden)) {
            notifyObserversFinishedHiding();
        }
    }

    @Override
    protected boolean shouldDelayHideAnimation(Layout layoutBeingHidden) {
        return false;
    }

    @Override
    protected boolean shouldShowToolbarAnimationOnShow(boolean isAnimate) {
        return isAnimate
                && (!mEnableAnimations || getTabModelSelector().getCurrentModel().getCount() <= 0);
    }

    @Override
    protected boolean shouldShowToolbarAnimationOnHide(Layout layoutBeingHidden, int nextTabId) {
        boolean showAnimation = true;
        return showAnimation;
    }

    @Override
    protected void tabCreated(int id, int sourceId, @TabLaunchType int launchType,
            boolean incognito, boolean willBeSelected, float originX, float originY) {
        Tab newTab = TabModelUtils.getTabById(getTabModelSelector().getModel(incognito), id);
        mCreatingNtp = newTab != null && newTab.isNativePage();
        super.tabCreated(id, sourceId, launchType, incognito, willBeSelected, originX, originY);
    }

    @Override
    public void onTabsAllClosing(boolean incognito) {
        if (!isOverviewLayout(getActiveLayout())) return;

        super.onTabsAllClosing(incognito);
    }

    /**
     * @return The {@link StripLayoutHelperManager} managed by this class.
     */
    @VisibleForTesting
    public StripLayoutHelperManager getStripLayoutHelperManager() {
        return null;
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
        return isOverviewLayout(activeLayout) && !activeLayout.isStartingToHide();
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
     * A {@link SwipeHandler} meant to respond to edge events for the toolbar.
     */
    protected class ToolbarSwipeHandler implements SwipeHandler {
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
        public void onSwipeStarted(@ScrollDirection int direction, MotionEvent ev) {
            mScrollDirection = ScrollDirection.UNKNOWN;
        }

        @Override
        public void onSwipeUpdated(MotionEvent current, float tx, float ty, float dx, float dy) {

        }

        @Override
        public void onSwipeFinished() {
        }

        @Override
        public void onFling(@ScrollDirection int direction, MotionEvent current, float tx, float ty,
                float vx, float vy) {
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
                return false;
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

        // Close the previous tab if the previous tab is a NTP.
        Tab lastTab = getTabById(lastTabId);
        if (UrlUtilities.isNTPUrl(lastTab.getUrl()) && !lastTab.canGoBack()
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
}
