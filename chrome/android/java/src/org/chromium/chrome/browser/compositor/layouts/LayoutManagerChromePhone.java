// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.layouts.phone.SimpleAnimationLayout;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * {@link LayoutManagerChromePhone} is the specialization of {@link LayoutManagerChrome} for the
 * phone.
 */
public class LayoutManagerChromePhone extends LayoutManagerChrome {
    // Layouts
    private SimpleAnimationLayout mSimpleAnimationLayout;

    /**
     * Creates an instance of a {@link LayoutManagerChromePhone}.
     * @param host         A {@link LayoutManagerHost} instance.
     * @param contentContainer A {@link ViewGroup} for Android views to be bound to.
     * @param startSurface An interface to talk to the Grid Tab Switcher. If it's NULL, VTS
     *                     should be used, otherwise GTS should be used.
     * @param tabContentManagerSupplier Supplier of the {@link TabContentManager} instance.
     * @param layerTitleCacheSupplier Supplier of the {@link LayerTitleCache}.
     * @param overviewModeBehaviorSupplier Supplier of the {@link OverviewModeBehavior}.
     * @param layoutStateProviderOneshotSupplier Supplier of the {@link LayoutStateProvider}.
     * @param topUiThemeColorProvider {@link ThemeColorProvider} for top UI.
     */
    public LayoutManagerChromePhone(LayoutManagerHost host, ViewGroup contentContainer,
            StartSurface startSurface,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            Supplier<LayerTitleCache> layerTitleCacheSupplier,
            OneshotSupplierImpl<OverviewModeBehavior> overviewModeBehaviorSupplier,
            OneshotSupplierImpl<LayoutStateProvider> layoutStateProviderOneshotSupplier,
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider) {
        super(host, contentContainer, true, startSurface, tabContentManagerSupplier,
                layerTitleCacheSupplier, overviewModeBehaviorSupplier,
                layoutStateProviderOneshotSupplier, topUiThemeColorProvider);
    }

    @Override
    public void init(TabModelSelector selector, TabCreatorManager creator,
            ControlContainer controlContainer, DynamicResourceLoader dynamicResourceLoader) {
        Context context = mHost.getContext();
        LayoutRenderHost renderHost = mHost.getLayoutRenderHost();

        // Build Layouts
        mSimpleAnimationLayout = new SimpleAnimationLayout(context, this, renderHost);

        super.init(selector, creator, controlContainer, dynamicResourceLoader);

        // Set up layout parameters
        mStaticLayout.setLayoutHandlesTabLifecycles(false);

        mToolbarSwipeLayout.setMovesToolbar(true);

        // Initialize Layouts
        TabContentManager tabContentManager = mTabContentManagerSupplier.get();
        assert tabContentManager != null;
        mSimpleAnimationLayout.setTabModelSelector(selector, tabContentManager);
    }

    @Override
    public boolean closeAllTabsRequest(boolean incognito) {
        if (getActiveLayout() == mStaticLayout && !incognito) {
            startShowing(DeviceClassManager.enableAccessibilityLayout() ? mOverviewListLayout
                                                                        : mOverviewLayout,
                    /* animate= */ false);
        }
        return super.closeAllTabsRequest(incognito);
    }

    @Override
    protected LayoutManagerTabModelObserver createTabModelObserver() {
        return new LayoutManagerTabModelObserver() {
            @Override
            public void willCloseTab(Tab tab, boolean animate) {
                super.willCloseTab(tab, animate);
                if (animate) tabClosing(tab.getId());
            }
        };
    }

    @Override
    protected void emptyCachesExcept(int tabId) {
        super.emptyCachesExcept(tabId);
        if (mTitleCache != null) mTitleCache.clearExcept(tabId);
    }

    private void tabClosing(int id) {
        Tab closedTab = getTabById(id);
        if (closedTab == null) return;

        if (getActiveLayout().handlesTabClosing()) {
            // The user is currently interacting with the {@code LayoutHost}.
            // Allow the foreground layout to animate the tab closing.
            getActiveLayout().onTabClosing(time(), id);
        } else if (animationsEnabled()) {
            startShowing(mSimpleAnimationLayout, false);
            getActiveLayout().onTabClosing(time(), id);
        }
    }

    @Override
    protected void tabClosed(int id, int nextId, boolean incognito, boolean tabRemoved) {
        boolean showOverview = nextId == Tab.INVALID_TAB_ID;
        Layout overviewLayout = DeviceClassManager.enableAccessibilityLayout() ? mOverviewListLayout
                                                                               : mOverviewLayout;
        if (getActiveLayout() != overviewLayout && showOverview) {
            // Since there will be no 'next' tab to display, switch to
            // overview mode when the animation is finished.
            setNextLayout(overviewLayout);
        }
        getActiveLayout().onTabClosed(time(), id, nextId, incognito);
        Tab nextTab = getTabById(nextId);
        if (nextTab != null && nextTab.getView() != null) nextTab.getView().requestFocus();
        boolean animate = !tabRemoved && animationsEnabled();
        if (getActiveLayout() != overviewLayout && showOverview && !animate) {
            showOverview(false);
        }
    }

    @Override
    protected void tabCreating(int sourceId, String url, boolean isIncognito) {
        if (getActiveLayout() != null && !getActiveLayout().isStartingToHide()
                && overlaysHandleTabCreating() && getActiveLayout().handlesTabCreating()) {
            // If the current layout in the foreground, let it handle the tab creation animation.
            // This check allows us to switch from the StackLayout to the SimpleAnimationLayout
            // smoothly.
            getActiveLayout().onTabCreating(sourceId);
        } else if (animationsEnabled()) {
            if (!overviewVisible()) {
                if (getActiveLayout() != null && getActiveLayout().isStartingToHide()) {
                    setNextLayout(mSimpleAnimationLayout);
                    // The method Layout#doneHiding() will automatically show the next layout.
                    getActiveLayout().doneHiding();
                } else {
                    startShowing(mSimpleAnimationLayout, false);
                }
            }
            if (getActiveLayout() != null) {
                getActiveLayout().onTabCreating(sourceId);
            }
        }
    }

    /** @return Whether the {@link SceneOverlay}s handle tab creation. */
    private boolean overlaysHandleTabCreating() {
        Layout layout = getActiveLayout();
        if (layout == null || layout.getLayoutTabsToRender() == null
                || layout.getLayoutTabsToRender().length != 1) {
            return false;
        }
        for (int i = 0; i < mSceneOverlays.size(); i++) {
            if (!mSceneOverlays.get(i).isSceneOverlayTreeShowing()) continue;
            if (mSceneOverlays.get(i).handlesTabCreating()) {
                // Prevent animation from happening if the overlay handles creation.
                startHiding(layout.getLayoutTabsToRender()[0].getId(), false);
                doneHiding();
                return true;
            }
        }
        return false;
    }

    @Override
    protected void tabCreated(int id, int sourceId, @TabLaunchType int launchType,
            boolean isIncognito, boolean willBeSelected, float originX, float originY) {
        super.tabCreated(id, sourceId, launchType, isIncognito, willBeSelected, originX, originY);

        if (willBeSelected) {
            Tab newTab = TabModelUtils.getTabById(getTabModelSelector().getModel(isIncognito), id);
            if (newTab != null && newTab.getView() != null) newTab.getView().requestFocus();
        }
    }

    @Override
    public void releaseTabLayout(int id) {
        mTitleCache.remove(id);
        super.releaseTabLayout(id);
    }
}
