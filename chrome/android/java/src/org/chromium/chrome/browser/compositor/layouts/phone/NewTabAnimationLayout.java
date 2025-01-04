// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import android.content.Context;
import android.graphics.RectF;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.BlackHoleEventFilter;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.sensitive_content.SensitiveContentClient;
import org.chromium.components.sensitive_content.SensitiveContentFeatures;
import org.chromium.ui.resources.ResourceManager;

import java.util.Collections;

/**
 * Layout for showing animations when new tabs are created. This is a drop-in replacement for the
 * {@link SimpleAnimationLayout} that uses Android animators rather than compositor animations and
 * uses modern UX designs.
 */
public class NewTabAnimationLayout extends Layout {
    private final ViewGroup mAnimationHostView;
    private final BlackHoleEventFilter mBlackHoleEventFilter;

    private @Nullable StaticTabSceneLayer mSceneLayer;
    private int mNextTabId = Tab.INVALID_TAB_ID;

    /**
     * Creates an instance of the {@link NewTabAnimationLayout}.
     *
     * @param context The current Android's context.
     * @param updateHost The {@link LayoutUpdateHost} view for this layout.
     * @param renderHost The {@link LayoutRenderHost} view for this layout.
     * @param animationHostView The host view to use for animations and content sensitivity.
     */
    public NewTabAnimationLayout(
            Context context,
            LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost,
            ViewGroup animationHostView) {
        super(context, updateHost, renderHost);
        mAnimationHostView = animationHostView;
        mBlackHoleEventFilter = new BlackHoleEventFilter(context);
    }

    @Override
    public void onFinishNativeInitialization() {
        ensureSceneLayerExists();
    }

    @Override
    public void destroy() {
        if (mSceneLayer != null) {
            mSceneLayer.destroy();
            mSceneLayer = null;
        }
    }

    @Override
    public void setTabContentManager(TabContentManager tabContentManager) {
        super.setTabContentManager(tabContentManager);
        if (mSceneLayer != null && tabContentManager != null) {
            mSceneLayer.setTabContentManager(tabContentManager);
        }
    }

    @Override
    public @ViewportMode int getViewportMode() {
        return ViewportMode.USE_PREVIOUS_BROWSER_CONTROLS_STATE;
    }

    @Override
    public boolean handlesTabCreating() {
        return true;
    }

    @Override
    public boolean handlesTabClosing() {
        return false;
    }

    @Override
    protected EventFilter getEventFilter() {
        return mBlackHoleEventFilter;
    }

    @Override
    public SceneLayer getSceneLayer() {
        return mSceneLayer;
    }

    @Override
    public int getLayoutType() {
        return LayoutType.SIMPLE_ANIMATION;
    }

    @Override
    public void show(long time, boolean animate) {
        super.show(time, animate);

        mNextTabId = Tab.INVALID_TAB_ID;
        reset();

        if (mTabModelSelector == null || mTabContentManager == null) return;

        @Nullable Tab tab = mTabModelSelector.getCurrentTab();
        if (tab != null && tab.isNativePage()) {
            mTabContentManager.cacheTabThumbnail(tab);
        }
    }

    @Override
    public void doneHiding() {
        TabModelUtils.selectTabById(mTabModelSelector, mNextTabId, TabSelectionType.FROM_USER);
        super.doneHiding();
        updateAnimationHostViewSensitivity(Tab.INVALID_TAB_ID);
    }

    @Override
    protected void forceAnimationToFinish() {
        // TODO(crbug.com/40282469): Implement this.
    }

    @Override
    public void onTabCreating(int sourceTabId) {
        reset();

        ensureSourceTabCreated(sourceTabId);
        updateAnimationHostViewSensitivity(sourceTabId);
    }

    @Override
    public void onTabCreated(
            long time,
            int id,
            int index,
            int sourceId,
            boolean newIsIncognito,
            boolean background,
            float originX,
            float originY) {
        assert mTabModelSelector != null;
        Tab newTab = mTabModelSelector.getModel(newIsIncognito).getTabById(id);
        if (newTab != null
                && newTab.getLaunchType() == TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP) {
            return;
        }

        ensureSourceTabCreated(sourceId);
        updateAnimationHostViewSensitivity(sourceId);

        // TODO(crbug.com/40282469): Implement this.
        // if (background) {
        // } else {
        // }
    }

    @Override
    protected void updateLayout(long time, long dt) {
        ensureSceneLayerExists();
        if (!hasLayoutTab()) return;

        boolean needUpdate = updateSnap(dt, getLayoutTab());
        if (needUpdate) requestUpdate();
    }

    @Override
    protected void updateSceneLayer(
            RectF viewport,
            RectF contentViewport,
            TabContentManager tabContentManager,
            ResourceManager resourceManager,
            BrowserControlsStateProvider browserControls) {
        ensureSceneLayerExists();

        LayoutTab layoutTab = getLayoutTab();
        layoutTab.set(LayoutTab.IS_ACTIVE_LAYOUT_SUPPLIER, this::isActive);
        layoutTab.set(LayoutTab.CONTENT_OFFSET, browserControls.getContentOffset());
        mSceneLayer.update(layoutTab);
    }

    private void reset() {
        mLayoutTabs = null;
    }

    private boolean hasLayoutTab() {
        return mLayoutTabs != null && mLayoutTabs.length > 0;
    }

    private LayoutTab getLayoutTab() {
        assert hasLayoutTab();
        return mLayoutTabs[0];
    }

    private void ensureSceneLayerExists() {
        if (mSceneLayer != null) return;

        mSceneLayer = new StaticTabSceneLayer();
        if (mTabContentManager == null) return;

        mSceneLayer.setTabContentManager(mTabContentManager);
    }

    private void ensureSourceTabCreated(int sourceTabId) {
        if (hasLayoutTab() && mLayoutTabs[0].getId() == sourceTabId) return;

        @Nullable Tab tab = mTabModelSelector.getTabById(sourceTabId);
        if (tab == null) return;
        LayoutTab sourceLayoutTab = createLayoutTab(sourceTabId, tab.isIncognitoBranded());

        mLayoutTabs = new LayoutTab[] {sourceLayoutTab};
        updateCacheVisibleIds(Collections.singletonList(sourceTabId));
    }

    private void updateAnimationHostViewSensitivity(int sourceTabId) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.VANILLA_ICE_CREAM
                || !ChromeFeatureList.isEnabled(SensitiveContentFeatures.SENSITIVE_CONTENT)
                || !ChromeFeatureList.isEnabled(
                        SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)) {
            return;
        }

        if (sourceTabId != TabModel.INVALID_TAB_INDEX) {
            // This code can be reached from both {@link SimpleAnimationLayout.onTabCreating}
            // and {@link SimpleAnimationLayout.onTabCreated}. If the content container is
            // already sensitive, there is no need to mark it as sensitive again.
            if (mAnimationHostView.getContentSensitivity() == View.CONTENT_SENSITIVITY_SENSITIVE) {
                return;
            }
            @Nullable Tab tab = mTabModelSelector.getTabById(sourceTabId);
            if (tab == null || !tab.getTabHasSensitiveContent()) {
                return;
            }
            mAnimationHostView.setContentSensitivity(View.CONTENT_SENSITIVITY_SENSITIVE);
            RecordHistogram.recordEnumeratedHistogram(
                    "SensitiveContent.SensitiveTabSwitchingAnimations",
                    SensitiveContentClient.TabSwitchingAnimation.NEW_TAB_IN_BACKGROUND,
                    SensitiveContentClient.TabSwitchingAnimation.COUNT);
        } else {
            mAnimationHostView.setContentSensitivity(View.CONTENT_SENSITIVITY_NOT_SENSITIVE);
        }
    }

    protected void setNextTabIdForTesting(int nextTabId) {
        mNextTabId = nextTabId;
    }
}
