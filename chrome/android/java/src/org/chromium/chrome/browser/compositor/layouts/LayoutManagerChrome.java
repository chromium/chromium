// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.MotionEvent;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.lifetime.DestroyChecker;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.hub.HubLayout;
import org.chromium.chrome.browser.hub.HubLayoutDependencyHolder;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabContentManager.ThumbnailChangeListener;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.ToolbarPositionController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.util.AccessibilityUtil;

import java.util.List;
import java.util.function.Supplier;

/**
 * A {@link Layout} controller for the more complicated Chrome browser. This is currently a superset
 * of {@link LayoutManagerImpl}.
 */
@NullMarked
public class LayoutManagerChrome extends LayoutManagerImpl implements AccessibilityUtil.Observer {
    // Layouts
    /** A {@link Layout} that should be used when the user is swiping sideways on the toolbar. */
    protected ToolbarSwipeLayout mToolbarSwipeLayout;

    /**
     * A {@link Layout} that should be used when the user is in the tab switcher when the hub flag
     * is enabled.
     */
    protected @Nullable Layout mHubLayout;

    // Event Filter Handlers
    private final SwipeHandler mToolbarSwipeHandler;

    /** Whether or not animations are enabled. This can disable certain layouts or effects. */
    private boolean mEnableAnimations = true;

    protected ObservableSupplier<TabContentManager> mTabContentManagerSupplier;
    private boolean mFinishNativeInitialization;
    private @Nullable TabContentManager mTabContentManager;

    // Lazy Tab Switcher Init
    private final Supplier<TabSwitcher> mTabSwitcherSupplier;
    private final Supplier<TabModelSelector> mTabModelSelectorSupplier;

    private final HubLayoutDependencyHolder mHubLayoutDependencyHolder;
    private final ThumbnailChangeListener mThumbnailChangeListener = (id) -> requestUpdate();
    private final Callback<TabContentManager> mOnTabContentManager = this::onTabContentManager;
    private final DestroyChecker mDestroyChecker = new DestroyChecker();

    protected @Nullable DesktopWindowStateManager mDesktopWindowStateManager;

    /**
     * Creates the {@link LayoutManagerChrome} instance.
     *
     * @param host A {@link LayoutManagerHost} instance.
     * @param contentContainer A {@link ViewGroup} for Android views to be bound to.
     * @param tabSwitcherSupplier Supplier for an interface to talk to the Grid Tab Switcher. Used
     *     to create TabSwitcherLayout if it has value.
     * @param tabModelSelectorSupplier Supplier for an interface to talk to the Tab Model Selector.
     * @param tabContentManagerSupplier Supplier of the {@link TabContentManager} instance.
     * @param topUiThemeColorProvider {@link ThemeColorProvider} for top UI.
     * @param hubLayoutDependencyHolder The dependency holder for creating {@link HubLayout}.
     */
    public LayoutManagerChrome(
            LayoutManagerHost host,
            ViewGroup contentContainer,
            Supplier<TabSwitcher> tabSwitcherSupplier,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider,
            HubLayoutDependencyHolder hubLayoutDependencyHolder) {
        super(host, contentContainer, tabContentManagerSupplier, topUiThemeColorProvider);
        // Build Event Filter Handlers
        mToolbarSwipeHandler =
                createToolbarSwipeHandler(/* supportsSwipeToShowTabSwitcher= */ true);

        mTabContentManagerSupplier = tabContentManagerSupplier;
        mTabContentManagerSupplier.addObserver(mOnTabContentManager);

        mTabSwitcherSupplier = tabSwitcherSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mHubLayoutDependencyHolder = hubLayoutDependencyHolder;
    }

    /** Creates {@link org.chromium.chrome.browser.hub.HubLayout}. */
    protected void createHubLayout(HubLayoutDependencyHolder hubLayoutDependencyHolder) {
        Context context = mHost.getContext();
        LayoutRenderHost renderHost = mHost.getLayoutRenderHost();

        mHubLayout =
                new HubLayout(
                        context,
                        /* updateHost= */ this,
                        renderHost,
                        /* layoutStateProvider= */ this,
                        hubLayoutDependencyHolder,
                        mTabModelSelectorSupplier,
                        mDesktopWindowStateManager);
        TabContentManager content = mTabContentManagerSupplier.get();
        if (content != null) {
            mHubLayout.setTabContentManager(content);
        }
        if (getTabModelSelector() != null) {
            mHubLayout.setTabModelSelector(getTabModelSelector());
        }
        if (mFinishNativeInitialization) {
            mHubLayout.onFinishNativeInitialization();
        }
    }

    /** Returns a list of virtual views representing compositor rendered views. */
    @Override
    public void getVirtualViews(List<VirtualView> views) {
        // TODO(dtrainor): Investigate order.
        for (int i = 0; i < mSceneOverlays.size(); i++) {
            if (!mSceneOverlays.get(i).isSceneOverlayTreeShowing()) continue;
            mSceneOverlays.get(i).getVirtualViews(views);
        }
    }

    /** Returns the {@link SwipeHandler} responsible for processing swipe events for the toolbar. */
    @Override
    public SwipeHandler getToolbarSwipeHandler() {
        return mToolbarSwipeHandler;
    }

    @Override
    public SwipeHandler createToolbarSwipeHandler(boolean supportsSwipeToShowTabSwitcher) {
        return new ToolbarSwipeHandler(supportsSwipeToShowTabSwitcher);
    }

    @Override
    @Initializer
    public void init(
            TabModelSelector selector,
            TabCreatorManager creator,
            @Nullable ControlContainer controlContainer,
            DynamicResourceLoader dynamicResourceLoader,
            TopUiThemeColorProvider topUiColorProvider,
            ObservableSupplier<Integer> bottomControlsOffsetSupplier) {
        Context context = mHost.getContext();
        LayoutRenderHost renderHost = mHost.getLayoutRenderHost();
        BrowserControlsStateProvider browserControlsStateProvider =
                mHost.getBrowserControlsManager();

        // Build Layouts
        mToolbarSwipeLayout =
                new ToolbarSwipeLayout(
                        context,
                        this,
                        renderHost,
                        browserControlsStateProvider,
                        this,
                        topUiColorProvider,
                        bottomControlsOffsetSupplier,
                        getContentContainer());

        super.init(
                selector,
                creator,
                controlContainer,
                dynamicResourceLoader,
                topUiColorProvider,
                bottomControlsOffsetSupplier);

        // Initialize Layouts
        TabContentManager content = mTabContentManagerSupplier.get();
        mToolbarSwipeLayout.setTabModelSelector(selector);
        mToolbarSwipeLayout.setTabContentManager(content);
        if (mHubLayout != null) {
            mHubLayout.setTabModelSelector(selector);
            mHubLayout.setTabContentManager(content);
            mHubLayout.onFinishNativeInitialization();
        }
        mFinishNativeInitialization = true;
    }

    @Override
    public void showLayout(int layoutType, boolean animate) {
        if (mDestroyChecker.isDestroyed()) return;

        if (layoutType == LayoutType.TAB_SWITCHER && mHubLayout == null) {
            initTabSwitcher();
        }
        super.showLayout(layoutType, animate);
    }

    /**
     * For lazy initialization of {@link HubLayout} This always happens the first time the tab
     * switcher is shown on tablets and phones.
     */
    private void initTabSwitcher() {
        var tabSwitcher = mTabSwitcherSupplier.get();
        if (tabSwitcher != null) {
            return;
        }

        if (mHubLayout == null) {
            createHubLayout(mHubLayoutDependencyHolder);
        }
    }

    @Override
    public void setTabModelSelector(TabModelSelector selector) {
        super.setTabModelSelector(selector);
        if (mHubLayout != null) {
            mHubLayout.setTabModelSelector(selector);
        }
    }

    @Override
    @SuppressWarnings("NullAway")
    public void destroy() {
        super.destroy();
        mDestroyChecker.destroy();

        if (mTabContentManagerSupplier != null) {
            mTabContentManagerSupplier.removeObserver(mOnTabContentManager);
            mTabContentManagerSupplier = null;
        }

        if (mTabContentManager != null) {
            mTabContentManager.removeThumbnailChangeListener(mThumbnailChangeListener);
            mTabContentManager = null;
        }

        if (mHubLayout != null) {
            mHubLayout.destroy();
            mHubLayout = null;
        }
        if (mToolbarSwipeLayout != null) {
            mToolbarSwipeLayout.destroy();
        }
    }

    @Override
    protected Layout getLayoutForType(@LayoutType int layoutType) {
        Layout layout = null;
        if (layoutType == LayoutType.TOOLBAR_SWIPE) {
            layout = mToolbarSwipeLayout;
        } else if (layoutType == LayoutType.TAB_SWITCHER) {
            if (mHubLayout != null) {
                layout = mHubLayout;
            }
        } else {
            layout = super.getLayoutForType(layoutType);
        }
        return assumeNonNull(layout);
    }

    @Override
    public void doneHiding() {
        if (getNextLayout() == getDefaultLayout()) {
            Tab tab = getTabModelSelector() != null ? getTabModelSelector().getCurrentTab() : null;
            emptyTabCachesExcept(tab != null ? tab.getId() : Tab.INVALID_TAB_ID);
        }

        super.doneHiding();
    }

    @Override
    protected void tabClosed(int id, int nextId, boolean incognito, boolean tabRemoved) {
        boolean showOverview = nextId == Tab.INVALID_TAB_ID;
        boolean animate = !tabRemoved && animationsEnabled();
        if (getActiveLayoutType() != LayoutType.TAB_SWITCHER
                && showOverview
                && getNextLayoutType() != LayoutType.TAB_SWITCHER
                && !DeviceInfo.isXr()) {
            showLayout(LayoutType.TAB_SWITCHER, animate);
        } else if (getActiveLayoutType() == LayoutType.TAB_SWITCHER
                && assumeNonNull(getActiveLayout()).isStartingToHide()
                && showOverview
                && getNextLayoutType() == LayoutType.BROWSING
                && !DeviceInfo.isXr()) {
            showLayout(LayoutType.TAB_SWITCHER, animate);
        }
        super.tabClosed(id, nextId, incognito, tabRemoved);
    }

    @Override
    public void tabsAllClosing(boolean incognito) {
        if (getActiveLayout() == mStaticLayout && !incognito && !DeviceInfo.isXr()) {
            showLayout(LayoutType.TAB_SWITCHER, /* animate= */ false);
        }
        super.tabsAllClosing(incognito);
    }

    @Override
    protected void tabModelSwitched(boolean incognito) {
        super.tabModelSwitched(incognito);
        TabModelSelector selector = getTabModelSelector();
        selector.commitAllTabClosures();

        // Skip forcing the tab switcher to show with 0 tabs until tab state is fully restored in
        // the event it is slow.
        TabModelUtils.runOnTabStateInitialized(
                selector,
                (tabModelSelector) -> {
                    boolean incognitoActive = tabModelSelector.isIncognitoBrandedModelSelected();

                    if (getActiveLayout() == mStaticLayout
                            && !incognitoActive
                            && tabModelSelector.getModel(false).getCount() == 0
                            && getNextLayoutType() != LayoutType.TAB_SWITCHER) {
                        showLayout(LayoutType.TAB_SWITCHER, /* animate= */ false);
                    }
                });
    }

    /** Initializes HubLayout without needing to open the Tab Switcher. */
    public void initHubLayoutForTesting() {
        initTabSwitcher();
    }

    /**
     * Returns the Hub {@link Layout} managed by this class. This should be non-null if init has
     * finished.
     */
    public @Nullable Layout getHubLayoutForTesting() {
        return mHubLayout;
    }

    /**
     * @param enabled Whether or not to allow model-reactive animations (tab creation, closing,
     *     etc.). Note that on an XR device the this param value is ignored and animation is
     *     disabled.
     */
    public void setEnableAnimations(boolean enabled) {
        mEnableAnimations = enabled;
    }

    /** Returns whether animations should be done for model changes. */
    @VisibleForTesting
    public boolean animationsEnabled() {
        return mEnableAnimations;
    }

    // ChromeAccessibilityUtil.Observer

    @Override
    public void onAccessibilityModeChanged(boolean enabled) {
        setEnableAnimations(DeviceClassManager.enableAnimations());
    }

    /** A {@link SwipeHandler} meant to respond to edge events for the toolbar. */
    protected class ToolbarSwipeHandler implements SwipeHandler {

        /** The scroll direction of the current gesture. */
        private @ScrollDirection int mScrollDirection;

        /**
         * The range in degrees that a swipe can be from a particular direction to be considered
         * that direction.
         */
        private static final float SWIPE_RANGE_DEG = 25;

        private final boolean mSupportsSwipeToShowTabSwitcher;

        public ToolbarSwipeHandler(boolean supportsSwipeToShowTabSwitcher) {
            mSupportsSwipeToShowTabSwitcher = supportsSwipeToShowTabSwitcher;
        }

        @Override
        public void onSwipeStarted(@ScrollDirection int direction, MotionEvent ev) {
            mScrollDirection = ScrollDirection.UNKNOWN;
        }

        @Override
        public void onSwipeUpdated(MotionEvent current, float tx, float ty, float dx, float dy) {
            if (mToolbarSwipeLayout == null) return;

            float x = current.getRawX() * mPxToDp;
            float y = current.getRawY() * mPxToDp;
            dx *= mPxToDp;
            dy *= mPxToDp;
            tx *= mPxToDp;
            ty *= mPxToDp;

            // If scroll direction has been computed, send the event to super.
            if (mScrollDirection != ScrollDirection.UNKNOWN) {
                mToolbarSwipeLayout.swipeUpdated(time(), x, y, dx, dy, tx, ty);
                return;
            }

            mScrollDirection = computeScrollDirection(dx, dy);
            if (mScrollDirection == ScrollDirection.UNKNOWN) return;

            if (mScrollDirection == ScrollDirection.LEFT
                    || mScrollDirection == ScrollDirection.RIGHT) {
                startShowing(mToolbarSwipeLayout, true);
            } else if (mSupportsSwipeToShowTabSwitcher) {
                // No need to test scroll direction here, as we've ruled out other possibilities.
                RecordUserAction.record("MobileToolbarSwipeOpenStackView");
                showLayout(LayoutType.TAB_SWITCHER, true);
            }

            mToolbarSwipeLayout.swipeStarted(time(), mScrollDirection, x, y);
        }

        @Override
        public void onSwipeFinished() {
            if (mToolbarSwipeLayout == null || !mToolbarSwipeLayout.isActive()) return;
            mToolbarSwipeLayout.swipeFinished(time());
        }

        @Override
        public void onFling(
                @ScrollDirection int direction,
                MotionEvent current,
                float tx,
                float ty,
                float vx,
                float vy) {
            if (mToolbarSwipeLayout == null || !mToolbarSwipeLayout.isActive()) return;
            float x = current.getRawX() * mPxToDp;
            float y = current.getRawX() * mPxToDp;
            tx *= mPxToDp;
            ty *= mPxToDp;
            vx *= mPxToDp;
            vy *= mPxToDp;
            mToolbarSwipeLayout.swipeFlingOccurred(time(), x, y, tx, ty, vx, vy);
        }

        /**
         * Compute the direction of the scroll.
         *
         * @param dx The distance traveled on the X axis.
         * @param dy The distance traveled on the Y axis.
         * @return The direction of the scroll.
         */
        private @ScrollDirection int computeScrollDirection(float dx, float dy) {
            @ScrollDirection int direction = ScrollDirection.UNKNOWN;

            // Figure out the angle of the swipe. Invert 'dy' so 90 degrees is up.
            double swipeAngle = (Math.toDegrees(Math.atan2(-dy, dx)) + 360) % 360;

            if (swipeAngle < 180 + SWIPE_RANGE_DEG && swipeAngle > 180 - SWIPE_RANGE_DEG) {
                direction = ScrollDirection.LEFT;
            } else if (swipeAngle < SWIPE_RANGE_DEG || swipeAngle > 360 - SWIPE_RANGE_DEG) {
                direction = ScrollDirection.RIGHT;
            } else if (swipeAngle < 270 + SWIPE_RANGE_DEG && swipeAngle > 270 - SWIPE_RANGE_DEG) {
                direction = ScrollDirection.DOWN;
            } else if (swipeAngle < 90 + SWIPE_RANGE_DEG && swipeAngle > 90 - SWIPE_RANGE_DEG) {
                direction = ScrollDirection.UP;
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

            Tab tab = getTabModelSelector() != null ? getTabModelSelector().getCurrentTab() : null;
            boolean toolbarShownOnTop = ToolbarPositionController.shouldShowToolbarOnTop(tab);
            @ScrollDirection
            int showTabSwitcherScrollDirection =
                    toolbarShownOnTop ? ScrollDirection.DOWN : ScrollDirection.UP;

            return direction == showTabSwitcherScrollDirection
                    || direction == ScrollDirection.LEFT
                    || direction == ScrollDirection.RIGHT;
        }
    }

    /**
     * @param id The id of the {@link Tab} to search for.
     * @return A {@link Tab} instance or {@code null} if it could be found.
     */
    protected @Nullable Tab getTabById(int id) {
        TabModelSelector selector = getTabModelSelector();
        return selector == null ? null : selector.getTabById(id);
    }

    @Override
    protected void switchToTab(@Nullable Tab tab, int lastTabId) {
        if (tab == null || lastTabId == Tab.INVALID_TAB_ID) {
            super.switchToTab(tab, lastTabId);
            return;
        }

        mToolbarSwipeLayout.setSwitchToTab(tab.getId(), lastTabId);
        showLayout(LayoutType.TOOLBAR_SWIPE, false);
    }

    private void onTabContentManager(TabContentManager tabContentManager) {
        assert mTabContentManager == null;
        mTabContentManager = tabContentManager;
        mTabContentManager.addThumbnailChangeListener(mThumbnailChangeListener);
        if (mHubLayout != null) {
            mHubLayout.setTabContentManager(mTabContentManager);
        }
        if (mTabContentManagerSupplier != null) {
            mTabContentManagerSupplier.removeObserver(mOnTabContentManager);
        }
    }
}
