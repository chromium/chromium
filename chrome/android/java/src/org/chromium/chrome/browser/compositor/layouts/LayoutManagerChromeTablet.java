// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.graphics.Bitmap;
import android.view.ViewGroup;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManagementDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.List;

/**
 * {@link LayoutManagerChromeTablet} is the specialization of {@link LayoutManagerChrome} for
 * the tablet.
 */
public class LayoutManagerChromeTablet extends LayoutManagerChrome {
    // Internal State
    private final String mDefaultTitle;

    private StripLayoutHelperManager mTabStripLayoutHelperManager;
    private TabModelSelectorTabObserver mTabObserver;

    /**
     * Creates an instance of a {@link LayoutManagerChromePhone}.
     * @param host                     A {@link LayoutManagerHost} instance.
     */
    public LayoutManagerChromeTablet(LayoutManagerHost host) {
        super(host, false, null);
        Context context = host.getContext();

        mTabStripLayoutHelperManager =
                new StripLayoutHelperManager(context, this, mHost.getLayoutRenderHost());

        // Set up state
        mDefaultTitle = context.getString(R.string.tab_loading_default_title);


        setNextLayout(null);
    }

    @Override
    protected void addAllSceneOverlays() {
        // Add the tab strip overlay before any others.
        addGlobalSceneOverlay(mTabStripLayoutHelperManager);
        super.addAllSceneOverlays();
    }

    @Override
    public void destroy() {
        super.destroy();

        if (mTabStripLayoutHelperManager != null) {
            mTabStripLayoutHelperManager.destroy();
            mTabStripLayoutHelperManager = null;
        }

        if (mTabObserver != null) {
            mTabObserver.destroy();
            mTabObserver = null;
        }
    }

    @Override
    public void tabSelected(int tabId, int prevId, boolean incognito) {
        if (getActiveLayout() == mStaticLayout || getActiveLayout() == mOverviewListLayout) {
            super.tabSelected(tabId, prevId, incognito);
        } else {
            startShowing(mStaticLayout, false);
            // TODO(dtrainor, jscholler): This is hacky because we're relying on it to set the
            // internal tab to show and not start hiding until we're done calling finalizeShowing().
            // This prevents a flicker because we properly build and set the internal
            // {@link LayoutTab} before actually showing the {@link TabView}.
            super.tabSelected(tabId, prevId, incognito);
            if (getActiveLayout() != null) getActiveLayout().onTabSelecting(time(), tabId);
        }
    }

    @Override
    protected void tabCreated(int id, int sourceId, @TabLaunchType int launchType,
            boolean incognito, boolean willBeSelected, float originX, float originY) {
        if (getFullscreenManager() != null) {
            getFullscreenManager().getBrowserVisibilityDelegate().showControlsTransient();
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
            TabContentManager content, ViewGroup androidContentContainer,
            ContextualSearchManagementDelegate contextualSearchDelegate,
            DynamicResourceLoader dynamicResourceLoader) {
        if (mTabStripLayoutHelperManager != null) {
            mTabStripLayoutHelperManager.setTabModelSelector(selector, creator);
        }

        super.init(selector, creator, content, androidContentContainer, contextualSearchDelegate,
                dynamicResourceLoader);

        mTabObserver = new TabModelSelectorTabObserver(selector) {
            @Override
            public void onFaviconUpdated(Tab tab, Bitmap icon) {
                updateTitle(tab);
            }

            @Override
            public void onTitleUpdated(Tab tab) {
                updateTitle(tab);
            }
        };

        // Make sure any tabs already restored get loaded into the title cache.
        List<TabModel> models = selector.getModels();
        for (int i = 0; i < models.size(); i++) {
            TabModel model = models.get(i);
            for (int j = 0; j < model.getCount(); j++) {
                Tab tab = model.getTabAt(j);
                if (tab != null && mTitleCache != null) {
                    mTitleCache.getUpdatedTitle(tab, mDefaultTitle);
                }
            }
        }
    }

    @Override
    protected LayoutManagerTabModelObserver createTabModelObserver() {
        return new LayoutManagerTabModelObserver() {
            @Override
            public void didAddTab(Tab tab, @TabLaunchType int launchType) {
                super.didAddTab(tab, launchType);
                updateTitle(getTabById(tab.getId()));
            }
        };
    }

    @Override
    public StripLayoutHelperManager getStripLayoutHelperManager() {
        return mTabStripLayoutHelperManager;
    }

    private void updateTitle(Tab tab) {
        if (tab != null && mTitleCache != null) {
            String title = mTitleCache.getUpdatedTitle(tab, mDefaultTitle);
            getActiveLayout().tabTitleChanged(tab.getId(), title);
        }
        requestUpdate();
    }
}
