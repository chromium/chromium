// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.view.ViewGroup;

import org.chromium.base.jank_tracker.JankTracker;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * {@link LayoutManagerChromeTablet} is the specialization of {@link LayoutManagerChrome} for
 * the tablet.
 */
public class LayoutManagerChromeTablet extends LayoutManagerChrome {

    private StripLayoutHelperManager mTabStripLayoutHelperManager;

    /**
     * Creates an instance of a {@link LayoutManagerChromePhone}.
     * @param host                     A {@link LayoutManagerHost} instance.
     * @param contentContainer A {@link ViewGroup} for Android views to be bound to.
     * @param startSurface An interface to talk to the Grid Tab Switcher.
     * @param tabContentManagerSupplier Supplier of the {@link TabContentManager} instance.
     * @param layerTitleCacheSupplier Supplier of the {@link LayerTitleCache}.
     * @param overviewModeBehaviorSupplier Supplier of the {@link OverviewModeBehavior}.
     * @param topUiThemeColorProvider {@link ThemeColorProvider} for top UI.
     */
    public LayoutManagerChromeTablet(LayoutManagerHost host, ViewGroup contentContainer,
            StartSurface startSurface,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            Supplier<LayerTitleCache> layerTitleCacheSupplier,
            OneshotSupplierImpl<OverviewModeBehavior> overviewModeBehaviorSupplier,
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider, JankTracker jankTracker) {
        super(host, contentContainer,
                TabUiFeatureUtilities.isGridTabSwitcherEnabled(host.getContext()), startSurface,
                tabContentManagerSupplier, layerTitleCacheSupplier, overviewModeBehaviorSupplier,
                topUiThemeColorProvider, jankTracker);

        mTabStripLayoutHelperManager = new StripLayoutHelperManager(host.getContext(), this,
                mHost.getLayoutRenderHost(), () -> mTitleCache, layerTitleCacheSupplier);
        addSceneOverlay(mTabStripLayoutHelperManager);

        setNextLayout(null);
    }

    @Override
    public void destroy() {
        super.destroy();

        if (mTabStripLayoutHelperManager != null) {
            mTabStripLayoutHelperManager.destroy();
            mTabStripLayoutHelperManager = null;
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

        if (mTabStripLayoutHelperManager != null) {
            mTabStripLayoutHelperManager.setTabModelSelector(selector, creator);
        }
    }

    @Override
    public StripLayoutHelperManager getStripLayoutHelperManager() {
        return mTabStripLayoutHelperManager;
    }
}
