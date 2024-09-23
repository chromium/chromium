// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.layouts.phone.SimpleAnimationLayout;
import org.chromium.chrome.browser.hub.HubLayoutDependencyHolder;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ControlContainer;
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
     *
     * @param host A {@link LayoutManagerHost} instance.
     * @param contentContainer A {@link ViewGroup} for Android views to be bound to.
     * @param tabSwitcherSupplier Supplier for an interface to talk to the Grid Tab Switcher. Used
     *     to create overviewLayout if it has value, otherwise will use the accessibility overview
     *     layout.
     * @param tabModelSelectorSupplier Supplier for an interface to talk to the Tab Model Selector.
     * @param tabContentManagerSupplier Supplier of the {@link TabContentManager} instance.
     * @param topUiThemeColorProvider {@link ThemeColorProvider} for top UI.
     * @param hubLayoutDependencyHolder The dependency holder for creating {@link HubLayout}.
     */
    public LayoutManagerChromePhone(
            LayoutManagerHost host,
            ViewGroup contentContainer,
            Supplier<TabSwitcher> tabSwitcherSupplier,
            Supplier<TabModelSelector> tabModelSelectorSupplier,
            ObservableSupplier<TabContentManager> tabContentManagerSupplier,
            Supplier<TopUiThemeColorProvider> topUiThemeColorProvider,
            HubLayoutDependencyHolder hubLayoutDependencyHolder) {
        super(
                host,
                contentContainer,
                tabSwitcherSupplier,
                tabModelSelectorSupplier,
                tabContentManagerSupplier,
                topUiThemeColorProvider,
                hubLayoutDependencyHolder);
    }

    @Override
    public void init(
            TabModelSelector selector,
            TabCreatorManager creator,
            ControlContainer controlContainer,
            DynamicResourceLoader dynamicResourceLoader,
            TopUiThemeColorProvider topUiColorProvider) {
        Context context = mHost.getContext();
        LayoutRenderHost renderHost = mHost.getLayoutRenderHost();

        // Build Layouts
        mSimpleAnimationLayout = new SimpleAnimationLayout(context, this, renderHost);

        super.init(selector, creator, controlContainer, dynamicResourceLoader, topUiColorProvider);

        // Initialize Layouts
        TabContentManager tabContentManager = mTabContentManagerSupplier.get();
        assert tabContentManager != null;
        mSimpleAnimationLayout.setTabModelSelector(selector);
        mSimpleAnimationLayout.setTabContentManager(tabContentManager);
    }

    @Override
    protected Layout getLayoutForType(int layoutType) {
        if (layoutType == LayoutType.SIMPLE_ANIMATION) {
            return mSimpleAnimationLayout;
        }
        return super.getLayoutForType(layoutType);
    }

    @Override
    public void onTabsAllClosing(boolean incognito) {
        if (getActiveLayout() == mStaticLayout && !incognito) {
            showLayout(LayoutType.TAB_SWITCHER, /* animate= */ false);
        }
        super.onTabsAllClosing(incognito);
    }

    @Override
    protected void tabClosed(int id, int nextId, boolean incognito, boolean tabRemoved) {
        boolean showOverview = nextId == Tab.INVALID_TAB_ID;
        if (getActiveLayoutType() != LayoutType.TAB_SWITCHER && showOverview) {
            // Since there will be no 'next' tab to display, switch to
            // overview mode when the animation is finished.
            if (getActiveLayoutType() == LayoutType.SIMPLE_ANIMATION) {
                setNextLayout(getLayoutForType(LayoutType.TAB_SWITCHER), true);
                getActiveLayout().onTabClosed(time(), id, nextId, incognito);
            } else {
                super.tabClosed(id, nextId, incognito, tabRemoved);
            }
        }
    }

    @Override
    protected void tabCreating(int sourceId, boolean isIncognito) {
        if (getActiveLayout() != null
                && !getActiveLayout().isStartingToHide()
                && overlaysHandleTabCreating()
                && getActiveLayout().handlesTabCreating()) {
            // If the current layout in the foreground, let it handle the tab creation animation.
            // This check allows us to switch from the StackLayout to the SimpleAnimationLayout
            // smoothly.
            getActiveLayout().onTabCreating(sourceId);
        } else if (animationsEnabled()) {
            if (!isLayoutVisible(LayoutType.TAB_SWITCHER)) {
                if (getActiveLayout() != null && getActiveLayout().isStartingToHide()) {
                    setNextLayout(mSimpleAnimationLayout, true);
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
        if (layout == null
                || layout.getLayoutTabsToRender() == null
                || layout.getLayoutTabsToRender().length != 1) {
            return false;
        }
        for (int i = 0; i < mSceneOverlays.size(); i++) {
            if (!mSceneOverlays.get(i).isSceneOverlayTreeShowing()) continue;
            if (mSceneOverlays.get(i).handlesTabCreating()) {
                // Prevent animation from happening if the overlay handles creation.
                startHiding();
                doneHiding();
                return true;
            }
        }
        return false;
    }
}
