// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility_tab_switcher;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.FrameLayout;
import android.widget.ListView;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.accessibility_tab_switcher.AccessibilityTabModelAdapter.AccessibilityTabModelAdapterListener;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.BlackHoleEventFilter;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilter;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.ui.base.DeviceFormFactor;

/**
 * A {@link Layout} that shows the tabs as two {@link ListView}s, one for each {@link TabModel} to
 * represent.
 */
public class OverviewListLayout extends Layout implements AccessibilityTabModelAdapterListener {
    private AccessibilityTabModelWrapper mTabModelWrapper;
    private final float mDensity;
    private final BlackHoleEventFilter mBlackHoleEventFilter;
    private final SceneLayer mSceneLayer;

    public OverviewListLayout(
            Context context, LayoutUpdateHost updateHost, LayoutRenderHost renderHost) {
        super(context, updateHost, renderHost);
        mBlackHoleEventFilter = new BlackHoleEventFilter(context);
        mDensity = context.getResources().getDisplayMetrics().density;
        mSceneLayer = new SceneLayer();
    }

    @Override
    public void attachViews(ViewGroup container) {
        if (mTabModelWrapper == null) {
            mTabModelWrapper =
                    (AccessibilityTabModelWrapper) LayoutInflater.from(container.getContext())
                            .inflate(R.layout.accessibility_tab_switcher, null);
            mTabModelWrapper.setup(this);
            mTabModelWrapper.setTabModelSelector(mTabModelSelector);
            adjustForFullscreen();
        }

        if (container == null || mTabModelWrapper.getParent() != null) return;

        ViewGroup overviewList =
                (ViewGroup) container.findViewById(R.id.overview_list_layout_holder);
        overviewList.setVisibility(View.VISIBLE);
        overviewList.addView(mTabModelWrapper);
    }

    @Override
    public @ViewportMode int getViewportMode() {
        return ViewportMode.ALWAYS_FULLSCREEN;
    }

    @Override
    protected void notifySizeChanged(float width, float height, int orientation) {
        adjustForFullscreen();
    }

    private void adjustForFullscreen() {
        if (mTabModelWrapper == null) return;
        FrameLayout.LayoutParams params =
                (FrameLayout.LayoutParams) mTabModelWrapper.getLayoutParams();
        if (params == null) return;

        params.bottomMargin = (int) (getBottomBrowserControlsHeight() * mDensity);
        params.topMargin = (int) (getTopBrowserControlsHeight() * mDensity);

        mTabModelWrapper.setLayoutParams(params);
    }

    @Override
    public boolean handlesTabClosing() {
        return true;
    }

    @Override
    public boolean handlesTabCreating() {
        return true;
    }

    @Override
    public boolean handlesCloseAll() {
        return true;
    }

    @Override
    public void onTabCreating(int sourceTabId) {
        super.onTabCreating(sourceTabId);
        startHiding(sourceTabId, false);
    }

    @Override
    public void onTabCreated(long time, int tabId, int tabIndex, int sourceTabId,
            boolean newIsIncognito, boolean background, float originX, float originY) {
        super.onTabCreated(
                time, tabId, tabIndex, sourceTabId, newIsIncognito, background, originX, originY);
        startHiding(tabId, false);
    }

    @Override
    public void onTabRestored(long time, int tabId) {
        super.onTabRestored(time, tabId);
        // Call show() so that new tabs and potentially the toggle between incognito and normal
        // lists are created.
        // TODO(twellington): add animation for showing the restored tab.
        show(time, false);
    }

    @Override
    public void onTabModelSwitched(boolean incognito) {
        super.onTabModelSwitched(incognito);
        if (mTabModelWrapper == null) return;
        mTabModelWrapper.setStateBasedOnModel();
    }

    @Override
    public void show(long time, boolean animate) {
        super.show(time, animate);
        if (mTabModelWrapper == null) return;
        mTabModelWrapper.setStateBasedOnModel();

        doneShowing();
    }

    @Override
    public void startHiding(int nextId, boolean hintAtTabSelection) {
        super.startHiding(nextId, hintAtTabSelection);

        doneHiding();
    }

    @Override
    public void detachViews() {
        if (mTabModelSelector != null) mTabModelSelector.commitAllTabClosures();
        if (mTabModelWrapper != null) {
            ViewGroup parent = (ViewGroup) mTabModelWrapper.getParent();
            if (parent != null) {
                parent.setVisibility(View.GONE);
                parent.removeView(mTabModelWrapper);
            }
        }
    }

    @Override
    public boolean canHostBeFocusable() {
        // TODO(https://crbug.com/918171): Consider fine-tuning accessibility support for the
        // overview list layout.
        // We don't allow the host to gain focus for phones so that the CompositorViewHolder doesn't
        // steal focus when trying to focus the disabled tab switcher button when there are no tabs
        // open (https://crbug.com/584423). This solution never worked on tablets, however, and
        // caused a different focus bug, so on tablets we do allow the host to gain focus
        // (https://crbug.com/925277).
        return DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext());
    }

    @Override
    public void setTabModelSelector(
            TabModelSelector tabModelSelector, TabContentManager tabContentManager) {
        super.setTabModelSelector(tabModelSelector, tabContentManager);

        if (mTabModelWrapper == null) return;
        mTabModelWrapper.setTabModelSelector(tabModelSelector);
    }

    @VisibleForTesting
    public AccessibilityTabModelWrapper getContainer() {
        return mTabModelWrapper;
    }

    @Override
    public void onTabsAllClosing(long time, boolean incognito) {
        super.onTabsAllClosing(time, incognito);

        TabModel model = mTabModelSelector.getModel(incognito);
        while (model.getCount() > 0) TabModelUtils.closeTabByIndex(model, 0);

        if (incognito) {
            mTabModelSelector.selectModel(!incognito);
        }
        if (mTabModelWrapper == null) return;
        mTabModelWrapper.setStateBasedOnModel();
    }

    @Override
    public void onTabClosureCommitted(long time, int tabId, boolean incognito) {
        mTabModelWrapper.setStateBasedOnModel();
    }

    @Override
    public void showTab(int tabId) {
        onTabSelecting(0, tabId);
    }

    @Override
    protected EventFilter getEventFilter() {
        return mBlackHoleEventFilter;
    }

    @Override
    protected SceneLayer getSceneLayer() {
        return mSceneLayer;
    }

    /**
     * Set whether or not the accessibility tab switcher is visible (from an accessibility
     * perspective), or whether it is obscured by another view.
     * @param isVisible Whether or not accessibility tab switcher is visible.
     */
    public void updateAccessibilityVisibility(boolean isVisible) {
        if (mTabModelWrapper == null) return;

        int importantForAccessibility = isVisible
                ? View.IMPORTANT_FOR_ACCESSIBILITY_AUTO
                : View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS;
        if (mTabModelWrapper.getImportantForAccessibility() != importantForAccessibility) {
            mTabModelWrapper.setImportantForAccessibility(importantForAccessibility);
            mTabModelWrapper.sendAccessibilityEvent(AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED);
        }
    }
}
