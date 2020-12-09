// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.view.ViewGroup;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.List;

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
     * @param tabContentManagerSupplier Supplier of the {@link TabContentManager} instance.
     * @param layerTitleCacheSupplier Supplier of the {@link LayerTitleCache}.
     * @param overviewModeBehaviorSupplier Supplier of the {@link OverviewModeBehavior}.
     * @param layoutStateProviderOneshotSupplier Supplier of the {@link LayoutStateProvider}.
     * @param topUiThemeColorProvider {@link ThemeColorProvider} for top UI.
     */
    public LayoutManagerChromeTablet(LayoutManagerHost host, ViewGroup contentContainer,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            Supplier<LayerTitleCache> layerTitleCacheSupplier,
            OneshotSupplierImpl<OverviewModeBehavior> overviewModeBehaviorSupplier,
            OneshotSupplierImpl<LayoutStateProvider> layoutStateProviderOneshotSupplier,
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider) {
        super(host, contentContainer, false, null, tabContentManagerSupplier,
                layerTitleCacheSupplier, overviewModeBehaviorSupplier,
                layoutStateProviderOneshotSupplier, topUiThemeColorProvider);

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
    protected void tabClosureCommitted(int id, boolean incognito) {
        super.tabClosureCommitted(id, incognito);
        if (mTitleCache != null) mTitleCache.remove(id);
    }

    @Override
    protected void tabModelSwitched(boolean incognito) {
        super.tabModelSwitched(incognito);
        getTabModelSelector().commitAllTabClosures();
    }

    @Override
    public void init(TabModelSelector selector, TabCreatorManager creator,
            ControlContainer controlContainer,
            DynamicResourceLoader dynamicResourceLoader) {
        if (mTabStripLayoutHelperManager != null) {
            mTabStripLayoutHelperManager.setTabModelSelector(selector, creator);
        }

        super.init(selector, creator, controlContainer, dynamicResourceLoader);

        // Make sure any tabs already restored get loaded into the title cache.
        List<TabModel> models = selector.getModels();
        for (int i = 0; i < models.size(); i++) {
            TabModel model = models.get(i);
            for (int j = 0; j < model.getCount(); j++) {
                Tab tab = model.getTabAt(j);
                if (tab != null && mTitleCache != null) {
                    mTitleCache.getUpdatedTitle(
                            tab, mHost.getContext().getString(R.string.tab_loading_default_title));
                }
            }
        }
    }

    @Override
    public StripLayoutHelperManager getStripLayoutHelperManager() {
        return mTabStripLayoutHelperManager;
    }
}
