// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager.TabModelStartupInfo;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.hub.HubLayoutDependencyHolder;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.concurrent.Callable;

/**
 * {@link LayoutManagerChromeTablet} is the specialization of {@link LayoutManagerChrome} for
 * the tablet.
 */
public class LayoutManagerChromeTablet extends LayoutManagerChrome {
    // Tab Strip
    private StripLayoutHelperManager mTabStripLayoutHelperManager;

    // Internal State
    /** A {@link TitleCache} instance that stores all title/favicon bitmaps as CC resources. */
    // This cache should not be cleared in LayoutManagerImpl#emptyCachesExcept(), since that method
    // is currently called when returning to the static layout, which is when these titles will be
    // visible. See https://crbug.com/1329293.
    protected LayerTitleCache mLayerTitleCache;

    /**
     * Creates an instance of a {@link LayoutManagerChromePhone}.
     *
     * @param host A {@link LayoutManagerHost} instance.
     * @param contentContainer A {@link ViewGroup} for Android views to be bound to.
     * @param startSurfaceSupplier Supplier for an interface to talk to the Grid Tab Switcher when
     *     Start surface refactor is disabled.
     * @param tabSwitcherSupplier Supplier for an interface to talk to the Grid Tab Switcher when
     *     Start surface refactor is enabled.
     * @param browserControlsStateProvider The {@link BrowserControlsStateProvider} for top
     *     controls.
     * @param tabContentManagerSupplier Supplier of the {@link TabContentManager} instance.
     * @param topUiThemeColorProvider {@link ThemeColorProvider} for top UI.
     * @param tabSwitcherViewHolder {@link ViewGroup} used by tab switcher layout to show scrim when
     *     overview is visible.
     * @param scrimCoordinator {@link ScrimCoordinator} to show/hide scrim.
     * @param lifecycleDispatcher @{@link ActivityLifecycleDispatcher} to be passed to TabStrip
     *     helper.
     * @param delayedTabSwitcherOrStartSurfaceCallable Callable to create StartSurface/GTS views.
     * @param hubLayoutDependencyHolder The dependency holder for creating {@link HubLayout}.
     * @param multiInstanceManager @{link MultiInstanceManager} passed to @{link StripLayoutHelper}
     *     to support tab drag and drop.
     * @param dragAndDropDelegate @{@link DragAndDropDelegate} passed to {@link
     *     StripLayoutHelperManager} to initiate tab drag and drop.
     * @param toolbarContainerView @{link View} passed to @{link StripLayoutHelper} to support tab
     *     drag and drop.
     * @param tabHoverCardViewStub The {@link ViewStub} representing the strip tab hover card.
     */
    public LayoutManagerChromeTablet(
            LayoutManagerHost host,
            ViewGroup contentContainer,
            Supplier<StartSurface> startSurfaceSupplier,
            Supplier<TabSwitcher> tabSwitcherSupplier,
            BrowserControlsStateProvider browserControlsStateProvider,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider,
            ObservableSupplier<TabModelStartupInfo> tabModelStartupInfoSupplier,
            ViewGroup tabSwitcherViewHolder,
            ScrimCoordinator scrimCoordinator,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            Callable<ViewGroup> delayedTabSwitcherOrStartSurfaceCallable,
            HubLayoutDependencyHolder hubLayoutDependencyHolder,
            MultiInstanceManager multiInstanceManager,
            DragAndDropDelegate dragAndDropDelegate,
            View toolbarContainerView,
            @NonNull ViewStub tabHoverCardViewStub,
            @NonNull WindowAndroid windowAndroid) {
        super(
                host,
                contentContainer,
                startSurfaceSupplier,
                tabSwitcherSupplier,
                browserControlsStateProvider,
                tabContentManagerSupplier,
                topUiThemeColorProvider,
                tabSwitcherViewHolder,
                scrimCoordinator,
                delayedTabSwitcherOrStartSurfaceCallable,
                hubLayoutDependencyHolder);
        mTabStripLayoutHelperManager =
                new StripLayoutHelperManager(
                        host.getContext(),
                        host,
                        this,
                        mHost.getLayoutRenderHost(),
                        () -> mLayerTitleCache,
                        tabModelStartupInfoSupplier,
                        lifecycleDispatcher,
                        multiInstanceManager,
                        dragAndDropDelegate,
                        toolbarContainerView,
                        tabHoverCardViewStub,
                        tabContentManagerSupplier,
                        browserControlsStateProvider,
                        windowAndroid);
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
    }

    @Override
    protected void tabCreated(
            int id,
            int sourceId,
            @TabLaunchType int launchType,
            boolean incognito,
            boolean willBeSelected,
            float originX,
            float originY) {
        if (getBrowserControlsManager() != null) {
            getBrowserControlsManager().getBrowserVisibilityDelegate().showControlsTransient();
        }
        super.tabCreated(id, sourceId, launchType, incognito, willBeSelected, originX, originY);
    }

    @Override
    public void onTabsAllClosing(boolean incognito) {
        if (getActiveLayout() == mStaticLayout && !incognito) {
            showLayout(LayoutType.TAB_SWITCHER, /* animate= */ false);
        }
        super.onTabsAllClosing(incognito);
    }

    @Override
    protected void tabModelSwitched(boolean incognito) {
        super.tabModelSwitched(incognito);
        getTabModelSelector().commitAllTabClosures();
        if (getActiveLayout() == mStaticLayout
                && !incognito
                && getTabModelSelector().getModel(false).getCount() == 0
                && getNextLayoutType() != LayoutType.TAB_SWITCHER) {
            showLayout(LayoutType.TAB_SWITCHER, /* animate= */ false);
        }
    }

    @Override
    public void init(
            TabModelSelector selector,
            TabCreatorManager creator,
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
