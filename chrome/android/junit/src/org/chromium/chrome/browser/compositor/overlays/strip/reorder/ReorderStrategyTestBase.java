// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.mockito.Mockito.when;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.animation.AnimatorSet;
import android.app.Activity;
import android.graphics.PointF;
import android.view.View;

import org.mockito.Mock;
import org.mockito.Spy;
import org.robolectric.Robolectric;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.overlays.strip.AnimationHost;
import org.chromium.chrome.browser.compositor.overlays.strip.ScrollDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.StripUpdateDelegate;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.browser.tasks.tab_management.ActionConfirmationManager;

import java.util.List;

public class ReorderStrategyTestBase {

    protected static final int TAB_WIDTH = 50;
    protected static final PointF DRAG_START_POINT = new PointF(70f, 20f); // Arbitrary value.
    protected static final float EPSILON = 0.001f;
    protected static final int INTERACTING_VIEW_ID = 10; // Arbitrary value.
    protected static final Token GROUP_ID =
            new Token(/* high= */ 0L, /* low= */ 0L); // Arbitrary value.

    // Dependencies
    private Activity mActivity;
    @Mock protected ActionConfirmationManager mActionConfirmationManager;
    @Mock protected ReorderStrategy mTabStrategy;
    @Mock protected ReorderStrategy mGroupStrategy;
    @Mock protected StripUpdateDelegate mStripUpdateDelegate;
    @Mock protected ScrollDelegate mScrollDelegate;
    @Mock protected View mContainerView;
    @Mock protected ObservableSupplierImpl<Integer> mGroupIdToHideSupplier;
    @Mock protected TabGroupModelFilter mTabGroupModelFilter;
    @Mock protected TabModel mModel;
    @Mock protected ReorderDelegate mReorderDelegate;
    @Mock protected Supplier<Float> mTabWidthSupplier;
    @Mock protected TabUngrouper mTabUnGrouper;
    @Spy protected AnimationHost mAnimationHost = new TestAnimationHost();

    // Data
    protected StripLayoutTab[] mStripTabs = new StripLayoutTab[0];
    protected StripLayoutGroupTitle[] mGroupTitles = new StripLayoutGroupTitle[0];
    protected StripLayoutView[] mStripViews = new StripLayoutView[0];
    protected StripLayoutTab mInteractingTab;
    protected StripLayoutGroupTitle mInteractingGroupTitle;
    protected StripLayoutGroupTitle mInteractingTabGroupTitle;
    @Mock protected Tab mTabForInteractingView;

    protected void setup() {
        mActivity = Robolectric.setupActivity(Activity.class);
        // StripLayoutViews need styles during initializations.
        mActivity.setTheme(org.chromium.chrome.R.style.Theme_BrowserUI);
        when(mModel.getTabById(INTERACTING_VIEW_ID)).thenReturn(mTabForInteractingView);
        when(mTabForInteractingView.getId()).thenReturn(INTERACTING_VIEW_ID);
    }

    protected StripLayoutGroupTitle buildGroupTitle(
            Integer rootId, Token groupId, int x, int width) {
        StripLayoutGroupTitle title =
                new StripLayoutGroupTitle(mActivity, null, false, rootId, groupId);
        setDrawProperties(title, x, width);
        return title;
    }

    protected StripLayoutTab buildStripTab(int id, int x, int width) {
        StripLayoutTab tab = new StripLayoutTab(mActivity, id, null, null, null, false);
        setDrawProperties(tab, x, width);
        return tab;
    }

    private void setDrawProperties(StripLayoutView view, int x, int width) {
        view.setDrawX(x);
        view.setDrawY(0);
        view.setHeight(40);
        view.setWidth(width);
        // Reset touch target inset to only use draw properties for position calculations.
        view.setTouchTargetInsets(0f, 0f, 0f, 0f);
        view.setVisible(true);
    }

    private static class TestAnimationHost implements AnimationHost {

        private CompositorAnimationHandler mHandler = new CompositorAnimationHandler(() -> {});
        private AnimatorSet mRunningAnimations = new AnimatorSet();

        TestAnimationHost() {
            CompositorAnimationHandler.setTestingMode(true);
        }

        @Override
        public CompositorAnimationHandler getAnimationHandler() {
            return mHandler;
        }

        @Override
        public void finishAnimations() {
            mRunningAnimations.end();
        }

        @Override
        public void finishAnimationsAndPushTabUpdates() {
            finishAnimations();
        }

        @Override
        public void startAnimations(List<Animator> animationList, AnimatorListener listener) {
            finishAnimations();
            mRunningAnimations.playTogether(animationList);
            if (listener != null) mRunningAnimations.addListener(listener);
            mRunningAnimations.start();
            // Immediately end to be able to verify end state.
            finishAnimations();
        }
    }
}
