// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.graphics.RectF;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.Layout.ViewportMode;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayer;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.resources.ResourceManager;

import java.util.Collections;

/**
 * A {@link Layout} for Hub that has an empty or single tab {@link SceneLayer}. Android UI for
 * a toolbar and panes will be rendered atop this layout.
 *
 * This implementation is a heavily modified fork of {@link TabSwitcherLayout} that will delegate
 * animations to the current pane.
 *
 * Normally, this layout will show an empty {@link SceneLayer}. However, to facilitate thumbnail
 * capture and animations it may transiently host a {@link StaticTabSceneLayer}.
 */
public class HubLayout extends Layout {
    private SceneLayer mCurrentSceneLayer;
    /** Scene layer to facilitate thumbnail capture prior to starting a transition animation. */
    private StaticTabSceneLayer mTabSceneLayer;
    /** An empty scene layer used to avoid drawing anything. */
    private SceneLayer mEmptySceneLayer;

    private final LayoutStateProvider mLayoutStateProvider;

    private HubLayoutAnimation mCurrentAnimation;

    /**
     * The previous {@link LayoutType}, valid between {@link #show(long, boolean)} and
     * {@link #doneShowing()}.
     */
    private @LayoutType int mPreviousLayoutType;

    /**
     * Create the {@link Layout} to show the Hub on.
     * @param context The current Android {@link Context}.
     * @param updateHost The {@link LayoutUpdateHost} for the {@link LayoutManager}.
     * @param renderHost The {@link LayoutRenderHost} for the {@link LayoutManager}.
     * @param layoutStateProvider The {@link LayoutStateProvider} for the {@link LayoutManager}.
     */
    public HubLayout(Context context, LayoutUpdateHost updateHost, LayoutRenderHost renderHost,
            LayoutStateProvider layoutStateProvider) {
        super(context, updateHost, renderHost);
        mLayoutStateProvider = layoutStateProvider;
    }

    /** Returns the current {@link HubLayoutAnimationType}. */
    @HubLayoutAnimationType
    int getCurrentAnimationType() {
        return mCurrentAnimation != null ? mCurrentAnimation.getAnimationType()
                                         : HubLayoutAnimationType.NONE;
    }

    // Layout.java Implementation:

    @Override
    public void onFinishNativeInitialization() {
        super.onFinishNativeInitialization();
        ensureSceneLayersExist();
    }

    @Override
    public void setTabContentManager(TabContentManager tabContentManager) {
        super.setTabContentManager(tabContentManager);
        if (mTabSceneLayer != null && mTabContentManager != null) {
            mTabSceneLayer.setTabContentManager(mTabContentManager);
        }
    }

    @Override
    public void destroy() {
        super.destroy();
        if (mTabSceneLayer != null) {
            mTabSceneLayer.destroy();
            mTabSceneLayer = null;
        }
        if (mEmptySceneLayer != null) {
            mEmptySceneLayer.destroy();
            mEmptySceneLayer = null;
        }
        mCurrentSceneLayer = null;
    }

    @Override
    protected void updateLayout(long time, long dt) {
        ensureSceneLayersExist();
        super.updateLayout(time, dt);
        if (!hasLayoutTab()) return;

        boolean needUpdate = updateSnap(dt, getLayoutTab());
        if (needUpdate) requestUpdate();
    }

    @Override
    public void contextChanged(Context context) {
        super.contextChanged(context);
        // This is called before show() and before getActiveLayoutType() changes so we can know what
        // layout the Hub is showing from.
        mPreviousLayoutType = mLayoutStateProvider.getActiveLayoutType();
    }

    @Override
    public @ViewportMode int getViewportMode() {
        // Hub has its own toolbar.
        // TODO(crbug/1487209): Confirm this doesn't cause the toolbar to disappear too early or
        // without animation.
        return ViewportMode.ALWAYS_FULLSCREEN;
    }

    @Override
    public void show(long time, boolean animate) {
        if (isStartingToShow()) return;

        super.show(time, animate);

        forceAnimationToFinish();

        final Tab currentTab = mTabModelSelector.getCurrentTab();
        createLayoutTabsForTab(currentTab);
        mCurrentSceneLayer = mTabSceneLayer;

        // TODO(crbug/1487209): Get the animations from a Pane or HubManager and forward some events
        // along so visibility and animation timing are synced.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
            mCurrentAnimation = new HubLayoutAnimation(HubLayoutAnimationType.TRANSLATE_UP);
        } else if (mPreviousLayoutType == LayoutType.START_SURFACE) {
            mCurrentAnimation = new HubLayoutAnimation(HubLayoutAnimationType.FADE_IN);
        } else {
            mCurrentAnimation = new HubLayoutAnimation(HubLayoutAnimationType.SHRINK_TAB);
        }

        // TODO(crbug/1487209): Make this transition async and play an animation before calling
        // doneShowing().
    }

    @Override
    public void doneShowing() {
        super.doneShowing();
        mCurrentSceneLayer = mEmptySceneLayer;
        mPreviousLayoutType = LayoutType.NONE;
        mCurrentAnimation = null;
        resetLayoutTabs();
    }

    @Override
    public void startHiding(int nextTabId, boolean hintAtTabSelection) {
        if (isStartingToHide()) return;

        super.startHiding(nextTabId, hintAtTabSelection);

        // Let the new tab animation handle the transition.
        if (getCurrentAnimationType() == HubLayoutAnimationType.NEW_TAB) return;

        forceAnimationToFinish();

        mCurrentSceneLayer = mEmptySceneLayer;

        @LayoutType
        int nextLayoutType = mLayoutStateProvider.getNextLayoutType();

        // TODO(crbug/1487209): Get the animations from a Pane or HubManager and forward some events
        // along so visibility and animation timing are synced.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
            mCurrentAnimation = new HubLayoutAnimation(HubLayoutAnimationType.TRANSLATE_DOWN);
        } else if (nextLayoutType == LayoutType.START_SURFACE) {
            mCurrentAnimation = new HubLayoutAnimation(HubLayoutAnimationType.FADE_OUT);
        } else {
            mCurrentAnimation = new HubLayoutAnimation(HubLayoutAnimationType.EXPAND_TAB);
        }

        // TODO(crbug/1487209): Make this transition async and play an animation before calling
        // doneHiding().
    }

    @Override
    public void doneHiding() {
        super.doneHiding();
        mCurrentAnimation = null;
    }

    @Override
    protected void forceAnimationToFinish() {
        if (mCurrentAnimation != null) {
            mCurrentAnimation.forceAnimationToFinish();
            mCurrentAnimation = null;
        }
    }

    @Override
    public boolean onBackPressed() {
        // TODO(crbug/1487209): Forward this to the HubManager. Legacy backpress handler soon to be
        // obsolete.
        return false;
    }

    @Override
    public void onTabCreated(long time, int tabId, int tabIndex, int sourceTabId,
            boolean newIsIncognito, boolean background, float originX, float originY) {
        super.onTabCreated(
                time, tabId, tabIndex, sourceTabId, newIsIncognito, background, originX, originY);

        // Tablet Hub doesn't handle new tab animations.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
            return;
        }

        forceAnimationToFinish();

        mCurrentSceneLayer = mEmptySceneLayer;

        mCurrentAnimation = new HubLayoutAnimation(HubLayoutAnimationType.NEW_TAB);

        // TODO(crbug/1487209): Make this transition async and play an animation before calling
        // doneHiding().
    }

    @Override
    protected boolean onUpdateAnimation(long time, boolean jumpToEnd) {
        // Return whether an animation is running. Ignore the inputs like TabSwitcherLayout.
        return mCurrentAnimation != null;
    }

    @Override
    public boolean handlesTabClosing() {
        // Tabs can be closed from the Tab Switcher panes without changing layout.
        return true;
    }

    @Override
    public boolean handlesTabCreating() {
        // For the new tab animation.
        return true;
    }

    @Override
    public boolean canHostBeFocusable() {
        // TODO(crbug/1487209): Consider returning false here so that the omnibox doesn't steal
        // focus.
        return super.canHostBeFocusable();
    }

    @Override
    protected EventFilter getEventFilter() {
        return null;
    }

    @Override
    protected SceneLayer getSceneLayer() {
        return mCurrentSceneLayer;
    }

    @Override
    protected void updateSceneLayer(RectF viewport, RectF contentViewport,
            TabContentManager tabContentManager, ResourceManager resourceManager,
            BrowserControlsStateProvider browserControls) {
        ensureSceneLayersExist();
        super.updateSceneLayer(
                viewport, contentViewport, tabContentManager, resourceManager, browserControls);

        if (mCurrentSceneLayer != mTabSceneLayer) return;

        LayoutTab layoutTab = getLayoutTab();
        layoutTab.set(LayoutTab.IS_ACTIVE_LAYOUT_SUPPLIER, this::isActive);
        layoutTab.set(LayoutTab.CONTENT_OFFSET, browserControls.getContentOffset());
        mTabSceneLayer.update(layoutTab);
    }

    @Override
    public @LayoutType int getLayoutType() {
        // Pretend to be the TAB_SWITCHER for initial development to minimize churn outside of
        // LayoutManager.
        return LayoutType.TAB_SWITCHER;
    }

    @Override
    public boolean isRunningAnimations() {
        return mCurrentAnimation != null;
    }

    // Internal helpers

    private void ensureSceneLayersExist() {
        if (mTabSceneLayer == null) {
            mTabSceneLayer = new StaticTabSceneLayer();
            if (mTabContentManager != null) {
                mTabSceneLayer.setTabContentManager(mTabContentManager);
            }
        }
        if (mEmptySceneLayer == null) {
            mEmptySceneLayer = new SceneLayer();
        }
        if (mCurrentSceneLayer == null && mEmptySceneLayer != null) {
            mCurrentSceneLayer = mEmptySceneLayer;
        }
    }

    private boolean hasLayoutTab() {
        return mLayoutTabs != null && mLayoutTabs.length > 0;
    }

    private LayoutTab getLayoutTab() {
        assert hasLayoutTab();
        return mLayoutTabs[0];
    }

    private void createLayoutTabsForTab(@Nullable Tab tab) {
        int tabId = getIdForTab(tab);
        LayoutTab layoutTab = createLayoutTab(tabId, mTabModelSelector.isIncognitoSelected());
        mLayoutTabs = new LayoutTab[] {layoutTab};
        updateCacheVisibleIds(Collections.singletonList(tabId));
    }

    private void resetLayoutTabs() {
        // Clear the visible IDs as once mLayoutTabs is empty tabs thumbnails cannot be captured.
        // This prevents thumbnail requests from waiting indefinitely.
        updateCacheVisibleIds(Collections.emptyList());

        // mLayoutTabs is used in conjunction with mTabSceneLayer to capture a tab thumbnail for
        // the last visible Tab prior to transitioning to the Hub. This should be nulled once
        // the capture is completed.
        mLayoutTabs = null;
    }

    /**
     * Returns the tab id for a {@link Tab}.
     * @param tab The {@link Tab} to get an ID for or null.
     * @return the {@code tab}'s ID or {@link Tab#INVALID_TAB_ID} if null.
     */
    private int getIdForTab(@Nullable Tab tab) {
        return tab == null ? Tab.INVALID_TAB_ID : tab.getId();
    }
}
