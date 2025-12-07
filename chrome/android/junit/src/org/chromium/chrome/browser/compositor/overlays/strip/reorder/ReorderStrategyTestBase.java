// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.overlays.strip.reorder;

import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.animation.Animator;
import android.animation.Animator.AnimatorListener;
import android.animation.AnimatorSet;
import android.app.Activity;
import android.graphics.PointF;
import android.view.View;

import org.junit.Rule;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Token;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.overlays.strip.AnimationHost;
import org.chromium.chrome.browser.compositor.overlays.strip.ScrollDelegate;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutGroupTitle;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutUtils;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.chrome.browser.compositor.overlays.strip.reorder.ReorderDelegate.StripUpdateDelegate;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.ActionConfirmationManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;

import java.util.List;
import java.util.function.Supplier;

public abstract class ReorderStrategyTestBase {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    // Constants
    protected static final float EPSILON = 0.001f;

    protected static final int TAB_WIDTH = 50;
    protected static final float EFFECTIVE_TAB_WIDTH =
            TAB_WIDTH - StripLayoutUtils.TAB_OVERLAP_WIDTH_DP;
    protected static final PointF DRAG_START_POINT = new PointF(70f, 20f);

    protected static final Token GROUP_ID1 = new Token(/* high= */ 1L, /* low= */ 1L);
    protected static final Token GROUP_ID2 = new Token(/* high= */ 2L, /* low= */ 2L);
    protected static final Token GROUP_ID3 = new Token(/* high= */ 3L, /* low= */ 3L);

    protected static final int TAB_ID1 = 1;
    protected static final int TAB_ID2 = 2;
    protected static final int TAB_ID3 = 3;
    protected static final int TAB_ID4 = 4;
    protected static final int TAB_ID5 = 5;
    protected static final int TAB_ID6 = 6;
    protected static final int[] TAB_IDS = {TAB_ID1, TAB_ID2, TAB_ID3, TAB_ID4, TAB_ID5, TAB_ID6};

    // Dependencies
    private Activity mActivity;
    protected MockTabModel mModel;
    @Mock protected Profile mProfile;
    @Mock protected ActionConfirmationManager mActionConfirmationManager;
    @Mock protected StripUpdateDelegate mStripUpdateDelegate;
    @Mock protected ScrollDelegate mScrollDelegate;
    @Mock protected View mContainerView;
    @Mock protected ObservableSupplierImpl<Token> mGroupIdToHideSupplier;
    @Mock protected TabGroupModelFilter mTabGroupModelFilter;
    @Mock protected ReorderDelegate mReorderDelegate;
    @Mock protected Supplier<Float> mTabWidthSupplier;
    @Mock protected Supplier<Long> mLastReorderScrollTimeSupplier;
    @Mock protected TabUngrouper mTabUnGrouper;
    @Spy protected AnimationHost mAnimationHost = new TestAnimationHost();

    // Data
    protected StripLayoutTab[] mStripTabs = new StripLayoutTab[0];
    protected StripLayoutGroupTitle[] mGroupTitles = new StripLayoutGroupTitle[0];
    protected StripLayoutView[] mStripViews = new StripLayoutView[0];

    protected StripLayoutTab mInteractingTab;
    protected StripLayoutGroupTitle mInteractingGroupTitle;

    // Captors
    @Captor ArgumentCaptor<List<Tab>> mTabListCaptor;
    @Captor ArgumentCaptor<Tab> mTabCaptor;

    protected void setup() {
        mActivity = Robolectric.setupActivity(Activity.class);
        // StripLayoutViews need styles during initializations.
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mModel = spy(new MockTabModel(mProfile, /* delegate= */ null));
        for (int id : TAB_IDS) mModel.addTab(id);
        mModel.setIndex(0, TabSelectionType.FROM_USER);

        when(mTabWidthSupplier.get()).thenReturn((float) TAB_WIDTH);
        when(mLastReorderScrollTimeSupplier.get()).thenReturn(0L);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mModel);
        when(mTabGroupModelFilter.getTabUngrouper()).thenReturn(mTabUnGrouper);
        setupStripViews();
    }

    protected abstract void setupStripViews();

    protected StripLayoutGroupTitle buildGroupTitle(Token groupId, int x) {
        StripLayoutGroupTitle title =
                new StripLayoutGroupTitle(mActivity, null, null, false, groupId);
        setDrawProperties(title, x);
        return title;
    }

    protected StripLayoutTab buildStripTab(int id, int x) {
        StripLayoutTab tab =
                new StripLayoutTab(mActivity, id, null, null, null, null, false, false);
        setDrawProperties(tab, x);
        return tab;
    }

    private void setDrawProperties(StripLayoutView view, int x) {
        view.setIdealX(x);
        view.setDrawX(x);
        view.setDrawY(0);
        view.setHeight(40);
        view.setWidth(TAB_WIDTH);
        // Reset touch target inset to only use draw properties for position calculations.
        view.setTouchTargetInsets(0f, 0f, 0f, 0f);
        view.setVisible(true);
    }

    protected void mockTabGroup(Token groupId, int rootId, Tab... tabs) {
        List<Tab> tabList = List.of(tabs);
        for (Tab tab : tabList) {
            when(mTabGroupModelFilter.isTabInTabGroup(tab)).thenReturn(true);
            when(mTabGroupModelFilter.getRelatedTabList(tab.getId())).thenReturn(tabList);
            when(mTabGroupModelFilter.getTabsInGroup(groupId)).thenReturn(tabList);
            tab.setTabGroupId(groupId);
            tab.setRootId(rootId);
        }
        when(mTabGroupModelFilter.getTabCountForGroup(groupId)).thenReturn(tabList.size());
        when(mTabGroupModelFilter.getGroupLastShownTabId(groupId))
                .thenReturn(tabList.get(0).getId());
    }

    private static class TestAnimationHost implements AnimationHost {

        private final CompositorAnimationHandler mHandler =
                new CompositorAnimationHandler(() -> {});
        private final AnimatorSet mRunningAnimations = new AnimatorSet();

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

        @Override
        public void queueAnimations(List<Animator> animationList, AnimatorListener listener) {
            startAnimations(animationList, listener);
        }
    }
}
