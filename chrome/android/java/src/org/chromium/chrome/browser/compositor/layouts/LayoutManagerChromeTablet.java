// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.view.ViewGroup;

import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeColorProvider.ThemeColorObserver;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.concurrent.Callable;

/**
 * {@link LayoutManagerChromeTablet} is the specialization of {@link LayoutManagerChrome} for
 * the tablet.
 */
public class LayoutManagerChromeTablet extends LayoutManagerChrome {
    // Tab Switcher
    private final JankTracker mJankTracker;
    private final ScrimCoordinator mScrimCoordinator;
    private final Callable<ViewGroup> mCreateStartSurfaceCallable;
    // Tab Strip
    private StripLayoutHelperManager mTabStripLayoutHelperManager;

    // Theme Color
    TopUiThemeColorProvider mTopUiThemeColorProvider;
    ThemeColorObserver mThemeColorObserver;

    // Internal State
    /** A {@link TitleCache} instance that stores all title/favicon bitmaps as CC resources. */
    // This cache should not be cleared in LayoutManagerImpl#emptyCachesExcept(), since that method
    // is currently called when returning to the static layout, which is when these titles will be
    // visible. See https://crbug.com/1329293.
    protected LayerTitleCache mLayerTitleCache;

    private final Supplier<StartSurface> mStartSurfaceSupplier;
    private final Supplier<TabSwitcher> mTabSwitcherSupplier;

    /**
     * Creates an instance of a {@link LayoutManagerChromePhone}.
     * @param host                     A {@link LayoutManagerHost} instance.
     * @param contentContainer A {@link ViewGroup} for Android views to be bound to.
     * @param startSurfaceSupplier Supplier for an interface to talk to the Grid Tab Switcher when
     *         Start surface refactor is disabled.
     * @param tabSwitcherSupplier Supplier for an interface to talk to the Grid Tab Switcher when
     *         Start surface refactor is enabled.
     * @param tabContentManagerSupplier Supplier of the {@link TabContentManager} instance.
     * @param topUiThemeColorProvider {@link ThemeColorProvider} for top UI.
     * @param jankTracker Tracker for surface jank.
     * @param tabSwitcherViewHolder {@link ViewGroup} used by tab switcher layout to show scrim
     *         when overview is visible.
     * @param scrimCoordinator {@link ScrimCoordinator} to show/hide scrim.
     * @param lifecycleDispatcher @{@link ActivityLifecycleDispatcher} to be passed to TabStrip
     *         helper.
     * @param delayedStartSurfaceCallable Callable to create StartSurface/GTS views.
     */
    public LayoutManagerChromeTablet(LayoutManagerHost host, ViewGroup contentContainer,
            Supplier<StartSurface> startSurfaceSupplier, Supplier<TabSwitcher> tabSwitcherSupplier,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider, JankTracker jankTracker,
            ViewGroup tabSwitcherViewHolder, ScrimCoordinator scrimCoordinator,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            Callable<ViewGroup> delayedStartSurfaceCallable) {
        super(host, contentContainer, startSurfaceSupplier, tabSwitcherSupplier,
                tabContentManagerSupplier, topUiThemeColorProvider, jankTracker,
                tabSwitcherViewHolder, scrimCoordinator);
        mStartSurfaceSupplier = startSurfaceSupplier;
        mTabSwitcherSupplier = tabSwitcherSupplier;
        mTabStripLayoutHelperManager = new StripLayoutHelperManager(host.getContext(), this,
                mHost.getLayoutRenderHost(), () -> mLayerTitleCache, lifecycleDispatcher);
        mJankTracker = jankTracker;
        mScrimCoordinator = scrimCoordinator;
        mCreateStartSurfaceCallable = delayedStartSurfaceCallable;
        addSceneOverlay(mTabStripLayoutHelperManager);
        addObserver(mTabStripLayoutHelperManager.getTabSwitcherObserver());

        setNextLayout(null, true);
    }

    @Override
    public void destroy() {
        super.destroy();

        if (mLayerTitleCache != null) {
            mLayerTitleCache.shutDown();
            mLayerTitleCache = null;
        }

        if (mTabStripLayoutHelperManager != null) {
            removeObserver(mTabStripLayoutHelperManager.getTabSwitcherObserver());
            mTabStripLayoutHelperManager.destroy();
            mTabStripLayoutHelperManager = null;
        }

        if (mTopUiThemeColorProvider != null && mThemeColorObserver != null) {
            mTopUiThemeColorProvider.removeThemeColorObserver(mThemeColorObserver);
            mTopUiThemeColorProvider = null;
            mThemeColorObserver = null;
        }
    }

    @Override
    protected void tabCreated(int id, int sourceId, @TabLaunchType int launchType,
            boolean incognito, boolean willBeSelected, float originX, float originY) {
        if (getBrowserControlsManager() != null) {
            getBrowserControlsManager().getBrowserVisibilityDelegate().showControlsTransient();
        }
        super.tabCreated(id, sourceId, launchType, incognito, willBeSelected, originX, originY);
    }

    @Override
    protected void tabModelSwitched(boolean incognito) {
        super.tabModelSwitched(incognito);
        getTabModelSelector().commitAllTabClosures();
    }

    @Override
    public void init(TabModelSelector selector, TabCreatorManager creator,
            ControlContainer controlContainer,
            DynamicResourceLoader dynamicResourceLoader,
            TopUiThemeColorProvider topUiColorProvider) {
        super.init(selector, creator, controlContainer, dynamicResourceLoader, topUiColorProvider);
        if (DeviceClassManager.enableLayerDecorationCache()) {
            mLayerTitleCache = new LayerTitleCache(mHost.getContext(), getResourceManager());
            // TODO: TitleCache should be a part of the ResourceManager.
            mLayerTitleCache.setTabModelSelector(selector);
        }

        if (mTabStripLayoutHelperManager != null) {
            mTabStripLayoutHelperManager.setTabModelSelector(selector, creator);
        }
    }

    @Override
    public void showLayout(int layoutType, boolean animate) {
        if (layoutType == LayoutType.TAB_SWITCHER && mOverviewLayout == null
                && mTabSwitcherLayout == null
                && TabUiFeatureUtilities.isTabletGridTabSwitcherEnabled(mHost.getContext())) {
            try {
                if (!mStartSurfaceSupplier.hasValue()) {
                    final ViewGroup containerView = mCreateStartSurfaceCallable.call();
                    createOverviewLayout(mStartSurfaceSupplier.get(), mTabSwitcherSupplier.get(),
                            mJankTracker, mScrimCoordinator, containerView);
                    if (TabUiFeatureUtilities.isTabletGridTabSwitcherPolishEnabled(
                                mHost.getContext())) {
                        mThemeColorObserver =
                                (color, shouldAnimate) -> containerView.setBackgroundColor(color);
                        mTopUiThemeColorProvider = getTopUiThemeColorProvider().get();
                        mTopUiThemeColorProvider.addThemeColorObserver(mThemeColorObserver);
                    }
                }
            } catch (Exception e) {
                throw new RuntimeException("Failed to initialize start surface.", e);
            }
        }
        super.showLayout(layoutType, animate);
    }

    @Override
    public void initLayoutTabFromHost(final int tabId) {
        if (mLayerTitleCache != null) {
            mLayerTitleCache.remove(tabId);
        }
        super.initLayoutTabFromHost(tabId);
    }

    @Override
    public void releaseTabLayout(int id) {
        mLayerTitleCache.remove(id);
        super.releaseTabLayout(id);
    }

    @Override
    public void releaseResourcesForTab(int tabId) {
        super.releaseResourcesForTab(tabId);
        mLayerTitleCache.remove(tabId);
    }

    @Override
    public StripLayoutHelperManager getStripLayoutHelperManager() {
        return mTabStripLayoutHelperManager;
    }
}
