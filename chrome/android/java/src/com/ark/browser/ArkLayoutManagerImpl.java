// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser;

import android.view.MotionEvent;

import androidx.annotation.VisibleForTesting;

import com.ark.browser.tab.PageCacheManager;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerHost;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.SwipeHandler;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.List;

/**
 * A {@link Layout} controller for the more complicated Chrome browser.  This is currently a
 * superset of {@link ArkLayoutManager}.
 */
public class ArkLayoutManagerImpl extends ArkLayoutManager
        implements OverviewModeBehavior, ChromeAccessibilityUtil.Observer {
    // Layouts

    /** Whether or not animations are enabled.  This can disable certain layouts or effects. */
    private boolean mEnableAnimations = true;
    private final ObserverList<OverviewModeObserver> mOverviewModeObservers;
    private LayoutStateObserver mTabSwitcherFocusLayoutStateObserver;

    /**
     * Creates the {@link ArkLayoutManagerImpl} instance.
     * @param host         A {@link LayoutManagerHost} instance.
     */
    public ArkLayoutManagerImpl(LayoutManagerHost host) {
        super(host);

        mOverviewModeObservers = new ObserverList<OverviewModeObserver>();
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

    @Override
    public void destroy() {
        super.destroy();
        mOverviewModeObservers.clear();

        if (mTabSwitcherFocusLayoutStateObserver != null) {
            removeObserver(mTabSwitcherFocusLayoutStateObserver);
            mTabSwitcherFocusLayoutStateObserver = null;
        }
    }

    private boolean isOverviewLayout(Layout layout) {
        return false;
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
    public void onTabsAllClosing(boolean incognito) {
        if (!isOverviewLayout(getActiveLayout())) return;

        super.onTabsAllClosing(incognito);
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

    private void notifyObserversStartedShowing(boolean showToolbar) {
        for (OverviewModeObserver overviewModeObserver : mOverviewModeObservers) {
            overviewModeObserver.onOverviewModeStartedShowing(showToolbar);
        }
    }

    private void notifyObserversFinishedShowing() {
        for (OverviewModeObserver overviewModeObserver : mOverviewModeObservers) {
            overviewModeObserver.onOverviewModeFinishedShowing();
        }
    }

    private void notifyObserversStartedHiding(boolean showToolbar, boolean creatingNtp) {
        for (OverviewModeObserver overviewModeObserver : mOverviewModeObservers) {
            overviewModeObserver.onOverviewModeStartedHiding(showToolbar, creatingNtp);
        }
    }

    private void notifyObserversFinishedHiding() {
        for (OverviewModeObserver overviewModeObserver : mOverviewModeObservers) {
            overviewModeObserver.onOverviewModeFinishedHiding();
        }
    }
}
