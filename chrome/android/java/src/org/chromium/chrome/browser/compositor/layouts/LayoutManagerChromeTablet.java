// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager.TabModelStartupInfo;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.hub.HubLayoutDependencyHolder;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.dragdrop.DragAndDropDelegate;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/** LayoutManagerChromeTablet is the specialization of LayoutManagerChrome for the tablet. */
public class LayoutManagerChromeTablet extends LayoutManagerChrome {
    // Tab Strip
    private StripLayoutHelperManager mTabStripLayoutHelperManager;

    // Internal State
    /** A LayerTitleCache instance that stores all title/favicon bitmaps as CC resources. */
    // This cache should not be cleared in LayoutManagerImpl#emptyCachesExcept(), since that method
    // is currently called when returning to the static layout, which is when these titles will be
    // visible. See https://crbug.com/1329293.
    protected LayerTitleCache mLayerTitleCache;

    protected ObservableSupplierImpl<LayerTitleCache> mLayerTitleCacheSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplier<Integer> mTabStripHeightSupplier;

    /**
     * Creates an instance of a LayoutManagerChromePhone.
     *
     * @param host A LayoutManagerHost instance.
     * @param contentContainer A ViewGroup for Android views to be bound to.
     * @param tabSwitcherSupplier Supplier for an interface to talk to the Grid Tab Switcher.
     * @param tabModelSelectorSupplier Supplier for an interface to talk to the Tab Model Selector.
     * @param browserControlsStateProvider The BrowserControlsStateProvider for top controls.
     * @param tabContentManagerSupplier Supplier of the TabContentManager instance.
     * @param topUiThemeColorProvider ThemeColorProvider for top UI.
     * @param lifecycleDispatcher ActivityLifecycleDispatcher to be passed to TabStrip helper.
     * @param hubLayoutDependencyHolder The dependency holder for creating HubLayout.
     * @param multiInstanceManager MultiInstanceManager passed to StripLayoutHelper to support tab
     *     drag and drop.
     * @param dragAndDropDelegate DragAndDropDelegate passed to StripLayoutHelperManager to initiate
     *     tab drag and drop.
     * @param toolbarContainerView View passed to StripLayoutHelper to support tab drag and drop.
     * @param tabHoverCardViewStub The ViewStub representing the strip tab hover card.
     * @param tabStripTooltipViewStub The ViewStub representing the tooltip for NTB or MSB.
     * @param toolbarManager The ToolbarManager instance.
     * @param desktopWindowStateManager The DesktopWindowStateManager for the app header.
     * @param actionConfirmationManager The {@link ActionConfirmationManager} for group actions.
     * @param dataSharingTabManager The {@link DataSharingTabManager} for shared groups.
     * @param bottomSheetController The {@link BottomSheetController} used to show bottom sheets.
     * @param shareDelegateSupplier Supplies {@link ShareDelegate} to share tab URLs.
     */
    public LayoutManagerChromeTablet(
            LayoutManagerHost host,
            ViewGroup contentContainer,
            Supplier<TabSwitcher> tabSwitcherSupplier,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            BrowserControlsStateProvider browserControlsStateProvider,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider,
            ObservableSupplier<TabModelStartupInfo> tabModelStartupInfoSupplier,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            HubLayoutDependencyHolder hubLayoutDependencyHolder,
            MultiInstanceManager multiInstanceManager,
            DragAndDropDelegate dragAndDropDelegate,
            View toolbarContainerView,
            @NonNull ViewStub tabHoverCardViewStub,
            @NonNull ViewStub tabStripTooltipViewStub,
            @NonNull WindowAndroid windowAndroid,
            @NonNull ToolbarManager toolbarManager,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            ActionConfirmationManager actionConfirmationManager,
            DataSharingTabManager dataSharingTabManager,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull Supplier<ShareDelegate> shareDelegateSupplier) {
        super(
                host,
                contentContainer,
                tabSwitcherSupplier,
                tabModelSelectorSupplier,
                tabContentManagerSupplier,
                topUiThemeColorProvider,
                hubLayoutDependencyHolder);
        mTabStripLayoutHelperManager =
                new StripLayoutHelperManager(
                        host.getContext(),
                        host,
                        this,
                        mHost.getLayoutRenderHost(),
                        mLayerTitleCacheSupplier,
                        tabModelStartupInfoSupplier,
                        lifecycleDispatcher,
                        multiInstanceManager,
                        dragAndDropDelegate,
                        toolbarContainerView,
                        tabHoverCardViewStub,
                        tabStripTooltipViewStub,
                        tabContentManagerSupplier,
                        browserControlsStateProvider,
                        windowAndroid,
                        toolbarManager,
                        desktopWindowStateManager,
                        actionConfirmationManager,
                        dataSharingTabManager,
                        bottomSheetController,
                        shareDelegateSupplier);
        addSceneOverlay(mTabStripLayoutHelperManager);
        addObserver(mTabStripLayoutHelperManager.getTabSwitcherObserver());
        mDesktopWindowStateManager = desktopWindowStateManager;
        mTabStripHeightSupplier = toolbarManager.getTabStripHeightSupplier();

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
    public void init(
            TabModelSelector selector,
            TabCreatorManager creator,
            ControlContainer controlContainer,
            DynamicResourceLoader dynamicResourceLoader,
            TopUiThemeColorProvider topUiColorProvider,
            ObservableSupplier<Integer> bottomControlsOffsetSupplier) {
        super.init(
                selector,
                creator,
                controlContainer,
                dynamicResourceLoader,
                topUiColorProvider,
                bottomControlsOffsetSupplier);
        if (DeviceClassManager.enableLayerDecorationCache()) {
            mLayerTitleCache =
                    new LayerTitleCache(
                            mHost.getContext(),
                            getResourceManager(),
                            mTabStripHeightSupplier.get());
            // TODO: TitleCache should be a part of the ResourceManager.
            mLayerTitleCache.setTabModelSelector(selector);
            mLayerTitleCacheSupplier.set(mLayerTitleCache);
        }

        if (mTabStripLayoutHelperManager != null) {
            mTabStripLayoutHelperManager.setTabModelSelector(selector, creator);
        }
    }

    @Override
    public void releaseResourcesForTab(int tabId) {
        super.releaseResourcesForTab(tabId);
        mLayerTitleCache.removeTabTitle(tabId);
    }

    @Override
    public StripLayoutHelperManager getStripLayoutHelperManager() {
        return mTabStripLayoutHelperManager;
    }

    @Override
    public boolean hasTabletUi() {
        return true;
    }
}
