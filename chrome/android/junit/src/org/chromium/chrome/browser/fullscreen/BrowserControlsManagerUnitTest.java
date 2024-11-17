// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.greaterThan;
import static org.hamcrest.Matchers.lessThan;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.app.Activity;
import android.content.res.Resources;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsOffsetHelper;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.ui.util.TokenHolder;

import java.util.Collections;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link BrowserControlsManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BrowserControlsManagerUnitTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();

    // Since these tests don't depend on the heights being pixels, we can use these as dpi directly.
    private static final int TOOLBAR_HEIGHT = 56;
    private static final int EXTRA_TOP_CONTROL_HEIGHT = 20;

    @Mock private Activity mActivity;
    @Mock private ControlContainer mControlContainer;
    @Mock private View mContainerView;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private Resources mResources;
    @Mock private BrowserControlsStateProvider.Observer mBrowserControlsStateProviderObserver;
    @Mock private Tab mTab;
    @Mock private ContentView mContentView;
    @Mock private TabModel mTabModel;
    @Mock private TabBrowserControlsOffsetHelper mTabBrowserControlsOffsetHelper;

    private @Captor ArgumentCaptor<Callback<Tab>> mCallbackTabCaptor;
    private @Captor ArgumentCaptor<TabModelObserver> mTabModelObserverCaptor;
    private @Captor ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private UserDataHost mUserDataHost = new UserDataHost();
    private BrowserControlsManager mBrowserControlsManager;
    private BrowserStateBrowserControlsVisibilityDelegate mControlsDelegate;

    @Before
    public void setUp() {
        ApplicationStatus.onStateChangeForTesting(mActivity, ActivityState.CREATED);
        when(mActivity.getResources()).thenReturn(mResources);
        when(mResources.getDimensionPixelSize(R.dimen.control_container_height))
                .thenReturn(TOOLBAR_HEIGHT);
        when(mControlContainer.getView()).thenReturn(mContainerView);

        // Only the last/current visibility matters and is verified by tests.
        doCallback(
                        (Integer visibility) ->
                                when(mContainerView.getVisibility()).thenReturn(visibility))
                .when(mContainerView)
                .setVisibility(anyInt());
        mContainerView.setVisibility(View.VISIBLE);

        when(mTab.isUserInteractable()).thenReturn(true);
        when(mTab.isInitialized()).thenReturn(true);
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTab.getContentView()).thenReturn(mContentView);
        doNothing().when(mContentView).removeOnHierarchyChangeListener(any());
        doNothing().when(mContentView).removeOnSystemUiVisibilityChangeListener(any());
        doNothing().when(mContentView).addOnHierarchyChangeListener(any());
        doNothing().when(mContentView).addOnSystemUiVisibilityChangeListener(any());
        when(mTabModelSelector.getModels()).thenReturn(Collections.singletonList(mTabModel));
        when(mTabModel.getComprehensiveModel()).thenReturn(mTabModel);

        BrowserControlsManager browserControlsManager =
                new BrowserControlsManager(
                        mActivity, BrowserControlsStateProvider.ControlsPosition.TOP);
        mBrowserControlsManager = spy(browserControlsManager);
        mBrowserControlsManager.initialize(
                mControlContainer,
                mActivityTabProvider,
                mTabModelSelector,
                R.dimen.control_container_height);
        mControlsDelegate = mBrowserControlsManager.getBrowserVisibilityDelegate();
        mBrowserControlsManager.addObserver(mBrowserControlsStateProviderObserver);
        when(mBrowserControlsManager.getTab()).thenReturn(mTab);
    }

    private void remakeWithoutSpy() {
        mBrowserControlsManager.destroy();
        mBrowserControlsManager =
                new BrowserControlsManager(
                        mActivity, BrowserControlsStateProvider.ControlsPosition.TOP);
        mBrowserControlsManager.initialize(
                mControlContainer,
                mActivityTabProvider,
                mTabModelSelector,
                R.dimen.control_container_height);
        mBrowserControlsManager.addObserver(mBrowserControlsStateProviderObserver);
        mControlsDelegate = mBrowserControlsManager.getBrowserVisibilityDelegate();

        doCallback((Runnable runnable) -> runnable.run())
                .when(mContainerView)
                .postOnAnimation(any());

        // TabBrowserControlsOffsetHelper casts to TabImpl which is package private, mock instead.
        mUserDataHost.setUserData(
                TabBrowserControlsOffsetHelper.USER_DATA_KEY, mTabBrowserControlsOffsetHelper);
    }

    private void notifyAddTab(Tab tab) {
        verify(mTabModel, atLeast(1)).addObserver(mTabModelObserverCaptor.capture());
        for (TabModelObserver observer : mTabModelObserverCaptor.getAllValues()) {
            observer.didAddTab(
                    tab,
                    TabLaunchType.FROM_LINK,
                    TabCreationState.LIVE_IN_FOREGROUND,
                    /* markedForSelection= */ false);
        }
    }

    private void notifyCurrentTab(Tab tab) {
        verify(mActivityTabProvider, atLeast(1)).addObserver(mCallbackTabCaptor.capture());
        for (Callback<Tab> observer : mCallbackTabCaptor.getAllValues()) {
            observer.onResult(tab);
        }
    }

    private void notifyContentViewScrollingStateChanged(boolean scrolling) {
        verify(mTab, atLeast(1)).addObserver(mTabObserverCaptor.capture());
        for (TabObserver observer : mTabObserverCaptor.getAllValues()) {
            observer.onContentViewScrollingStateChanged(scrolling);
        }
    }

    private void notifyBrowserControlsOffsetChanged(
            int topControlsOffsetY, int bottomControlsOffsetY) {
        verify(mTab, atLeast(1)).addObserver(mTabObserverCaptor.capture());
        for (TabObserver observer : mTabObserverCaptor.getAllValues()) {
            observer.onBrowserControlsOffsetChanged(
                    mTab,
                    topControlsOffsetY,
                    bottomControlsOffsetY,
                    /* contentOffsetY= */ 0,
                    /* topControlsMinHeightOffsetY= */ 0,
                    /* bottomControlsMinHeightOffsetY= */ 0);
        }
    }

    @Test
    public void testInitialTopControlsHeight() {
        assertEquals(
                "Wrong initial top controls height.",
                TOOLBAR_HEIGHT,
                mBrowserControlsManager.getTopControlsHeight());
    }

    @Test
    public void testListenersNotifiedOfTopControlsHeightChange() {
        final int topControlsHeight = TOOLBAR_HEIGHT + EXTRA_TOP_CONTROL_HEIGHT;
        final int topControlsMinHeight = EXTRA_TOP_CONTROL_HEIGHT;
        mBrowserControlsManager.setTopControlsHeight(topControlsHeight, topControlsMinHeight);
        verify(mBrowserControlsStateProviderObserver)
                .onTopControlsHeightChanged(topControlsHeight, topControlsMinHeight);
    }

    @Test
    public void testBrowserDrivenHeightIncreaseAnimation() {
        final int topControlsHeight = TOOLBAR_HEIGHT + EXTRA_TOP_CONTROL_HEIGHT;
        final int topControlsMinHeight = EXTRA_TOP_CONTROL_HEIGHT;

        // Simulate that we can't animate native browser controls.
        when(mBrowserControlsManager.getTab()).thenReturn(null);
        mBrowserControlsManager.setAnimateBrowserControlsHeightChanges(true);
        mBrowserControlsManager.setTopControlsHeight(topControlsHeight, topControlsMinHeight);

        assertNotEquals(
                "Min-height offset shouldn't immediately change.",
                topControlsMinHeight,
                mBrowserControlsManager.getTopControlsMinHeightOffset());
        assertNotNull(
                "Animator should be initialized.",
                mBrowserControlsManager.getControlsAnimatorForTesting());

        for (long time = 50;
                time < mBrowserControlsManager.getControlsAnimationDurationMsForTesting();
                time += 50) {
            int previousMinHeightOffset = mBrowserControlsManager.getTopControlsMinHeightOffset();
            int previousContentOffset = mBrowserControlsManager.getContentOffset();
            mBrowserControlsManager.getControlsAnimatorForTesting().setCurrentPlayTime(time);
            assertThat(
                    mBrowserControlsManager.getTopControlsMinHeightOffset(),
                    greaterThan(previousMinHeightOffset));
            assertThat(
                    mBrowserControlsManager.getContentOffset(), greaterThan(previousContentOffset));
        }

        mBrowserControlsManager.getControlsAnimatorForTesting().end();
        assertEquals(
                "Min-height offset should be equal to min-height after animation.",
                mBrowserControlsManager.getTopControlsMinHeightOffset(),
                topControlsMinHeight);
        assertEquals(
                "Content offset should be equal to controls height after animation.",
                mBrowserControlsManager.getContentOffset(),
                topControlsHeight);
        assertNull(mBrowserControlsManager.getControlsAnimatorForTesting());
    }

    @Test
    public void testBrowserDrivenHeightDecreaseAnimation() {
        // Simulate that we can't animate native browser controls.
        when(mBrowserControlsManager.getTab()).thenReturn(null);

        mBrowserControlsManager.setTopControlsHeight(
                TOOLBAR_HEIGHT + EXTRA_TOP_CONTROL_HEIGHT, EXTRA_TOP_CONTROL_HEIGHT);

        mBrowserControlsManager.setAnimateBrowserControlsHeightChanges(true);
        mBrowserControlsManager.setTopControlsHeight(TOOLBAR_HEIGHT, 0);

        assertNotEquals(
                "Min-height offset shouldn't immediately change.",
                0,
                mBrowserControlsManager.getTopControlsMinHeightOffset());
        assertNotNull(
                "Animator should be initialized.",
                mBrowserControlsManager.getControlsAnimatorForTesting());

        for (long time = 50;
                time < mBrowserControlsManager.getControlsAnimationDurationMsForTesting();
                time += 50) {
            int previousMinHeightOffset = mBrowserControlsManager.getTopControlsMinHeightOffset();
            int previousContentOffset = mBrowserControlsManager.getContentOffset();
            mBrowserControlsManager.getControlsAnimatorForTesting().setCurrentPlayTime(time);
            assertThat(
                    mBrowserControlsManager.getTopControlsMinHeightOffset(),
                    lessThan(previousMinHeightOffset));
            assertThat(mBrowserControlsManager.getContentOffset(), lessThan(previousContentOffset));
        }

        mBrowserControlsManager.getControlsAnimatorForTesting().end();
        assertEquals(
                "Min-height offset should be equal to the min-height after animation.",
                mBrowserControlsManager.getTopControlsMinHeight(),
                mBrowserControlsManager.getTopControlsMinHeightOffset());
        assertEquals(
                "Content offset should be equal to controls height after animation.",
                mBrowserControlsManager.getTopControlsHeight(),
                mBrowserControlsManager.getContentOffset());
        assertNull(mBrowserControlsManager.getControlsAnimatorForTesting());
    }

    @Test
    public void testChangeTopHeightWithoutAnimation_Browser() {
        // Simulate that we can't animate native browser controls.
        when(mBrowserControlsManager.getTab()).thenReturn(null);

        // Increase the height.
        mBrowserControlsManager.setTopControlsHeight(
                TOOLBAR_HEIGHT + EXTRA_TOP_CONTROL_HEIGHT, EXTRA_TOP_CONTROL_HEIGHT);

        verify(mBrowserControlsManager).showAndroidControls(false);
        assertEquals(
                "Controls should be fully shown after changing the height.",
                TOOLBAR_HEIGHT + EXTRA_TOP_CONTROL_HEIGHT,
                mBrowserControlsManager.getContentOffset());
        assertEquals(
                "Controls should be fully shown after changing the height.",
                0,
                mBrowserControlsManager.getTopControlOffset());
        assertEquals(
                "Min-height offset should be equal to the min-height after height changes.",
                EXTRA_TOP_CONTROL_HEIGHT,
                mBrowserControlsManager.getTopControlsMinHeightOffset());

        // Decrease the height.
        mBrowserControlsManager.setTopControlsHeight(TOOLBAR_HEIGHT, 0);

        // Controls should be fully shown after changing the height.
        verify(mBrowserControlsManager, times(2)).showAndroidControls(false);
        assertEquals(
                "Controls should be fully shown after changing the height.",
                TOOLBAR_HEIGHT,
                mBrowserControlsManager.getContentOffset());
        assertEquals(
                "Controls should be fully shown after changing the height.",
                0,
                mBrowserControlsManager.getTopControlOffset());
        assertEquals(
                "Min-height offset should be equal to the min-height after height changes.",
                0,
                mBrowserControlsManager.getTopControlsMinHeightOffset());
    }

    @Test
    public void testChangeTopHeightWithoutAnimation_Native() {
        int contentOffset = mBrowserControlsManager.getContentOffset();
        int controlOffset = mBrowserControlsManager.getTopControlOffset();
        int minHeightOffset = mBrowserControlsManager.getTopControlsMinHeightOffset();

        // Increase the height.
        mBrowserControlsManager.setTopControlsHeight(
                TOOLBAR_HEIGHT + EXTRA_TOP_CONTROL_HEIGHT, EXTRA_TOP_CONTROL_HEIGHT);

        // Controls visibility and offsets should be managed by native.
        verify(mBrowserControlsManager, never()).showAndroidControls(anyBoolean());
        assertEquals(
                "Content offset should have the initial value before round-trip to native.",
                contentOffset,
                mBrowserControlsManager.getContentOffset());
        assertEquals(
                "Controls offset should have the initial value before round-trip to native.",
                controlOffset,
                mBrowserControlsManager.getTopControlOffset());
        assertEquals(
                "Min-height offset should have the initial value before round-trip to native.",
                minHeightOffset,
                mBrowserControlsManager.getTopControlsMinHeightOffset());

        verify(mBrowserControlsStateProviderObserver)
                .onTopControlsHeightChanged(
                        TOOLBAR_HEIGHT + EXTRA_TOP_CONTROL_HEIGHT, EXTRA_TOP_CONTROL_HEIGHT);

        contentOffset = TOOLBAR_HEIGHT + EXTRA_TOP_CONTROL_HEIGHT;
        controlOffset = 0;
        minHeightOffset = EXTRA_TOP_CONTROL_HEIGHT;

        // Simulate the offset coming from cc::BrowserControlsOffsetManager.
        mBrowserControlsManager
                .getTabControlsObserverForTesting()
                .onBrowserControlsOffsetChanged(
                        mTab, controlOffset, 0, contentOffset, minHeightOffset, 0);

        // Decrease the height.
        mBrowserControlsManager.setTopControlsHeight(TOOLBAR_HEIGHT, 0);

        // Controls visibility and offsets should be managed by native.
        verify(mBrowserControlsManager, never()).showAndroidControls(anyBoolean());
        assertEquals(
                "Controls should be fully shown after getting the offsets from native.",
                contentOffset,
                mBrowserControlsManager.getContentOffset());
        assertEquals(
                "Controls should be fully shown after getting the offsets from native.",
                controlOffset,
                mBrowserControlsManager.getTopControlOffset());
        assertEquals(
                "Min-height offset should be equal to the min-height"
                        + " after getting the offsets from native.",
                minHeightOffset,
                mBrowserControlsManager.getTopControlsMinHeightOffset());

        verify(mBrowserControlsStateProviderObserver).onTopControlsHeightChanged(TOOLBAR_HEIGHT, 0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
    public void testShowAndroidControlsObserver() {
        remakeWithoutSpy();

        int token =
                mBrowserControlsManager.hideAndroidControlsAndClearOldToken(
                        TokenHolder.INVALID_TOKEN);
        verify(mContainerView).setVisibility(View.INVISIBLE);
        verify(mBrowserControlsStateProviderObserver)
                .onAndroidControlsVisibilityChanged(View.INVISIBLE);

        mBrowserControlsManager.releaseAndroidControlsHidingToken(token);
        assertEquals(View.VISIBLE, mContainerView.getVisibility());
        verify(mBrowserControlsStateProviderObserver)
                .onAndroidControlsVisibilityChanged(View.VISIBLE);
    }

    @Test
    public void testGetAndroidControlsVisibility() {
        BrowserControlsManager browserControlsManager =
                new BrowserControlsManager(
                        mActivity, BrowserControlsStateProvider.ControlsPosition.TOP);
        assertEquals(View.INVISIBLE, browserControlsManager.getAndroidControlsVisibility());

        browserControlsManager.initialize(
                mControlContainer,
                mActivityTabProvider,
                mTabModelSelector,
                R.dimen.control_container_height);
        assertEquals(View.VISIBLE, browserControlsManager.getAndroidControlsVisibility());

        mContainerView.setVisibility(View.INVISIBLE);
        assertEquals(View.INVISIBLE, browserControlsManager.getAndroidControlsVisibility());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
    public void testScrollingVisibility() {
        remakeWithoutSpy();
        assertEquals(View.VISIBLE, mBrowserControlsManager.getAndroidControlsVisibility());

        // Emit tab event such that we get an active tab observer.
        notifyAddTab(mTab);
        notifyCurrentTab(mTab);

        // Wait for SHOWN otherwise the optimization doesn't take effect.
        ShadowLooper.idleMainLooper(
                BrowserStateBrowserControlsVisibilityDelegate.MINIMUM_SHOW_DURATION_MS,
                TimeUnit.MILLISECONDS);
        assertEquals(BrowserControlsState.BOTH, mControlsDelegate.get().intValue());

        // Hide should be eagerly be reacted to, regardless of scrolling or not.
        notifyContentViewScrollingStateChanged(true);
        int token =
                mBrowserControlsManager.hideAndroidControlsAndClearOldToken(
                        TokenHolder.INVALID_TOKEN);
        assertEquals(View.INVISIBLE, mBrowserControlsManager.getAndroidControlsVisibility());

        // This should not cause visibility updates because we're scrolling.
        mBrowserControlsManager.releaseAndroidControlsHidingToken(token);

        // Now stop scrolling, and the visibility should update.
        notifyContentViewScrollingStateChanged(false);
        assertEquals(View.VISIBLE, mBrowserControlsManager.getAndroidControlsVisibility());

        // Set up the same situation where we're scrolling and have a hidden view that wants to be
        // shown once the scroll is over.
        notifyContentViewScrollingStateChanged(true);
        token =
                mBrowserControlsManager.hideAndroidControlsAndClearOldToken(
                        TokenHolder.INVALID_TOKEN);
        mBrowserControlsManager.releaseAndroidControlsHidingToken(token);
        assertEquals(View.INVISIBLE, mBrowserControlsManager.getAndroidControlsVisibility());

        // But now switch tabs instead. The manager should clear the scrolling signal. Although this
        // is actually the same tab object, nothing is doing an equality check.
        for (Callback<Tab> observer : mCallbackTabCaptor.getAllValues()) {
            observer.onResult(mTab);
        }
        assertEquals(View.VISIBLE, mBrowserControlsManager.getAndroidControlsVisibility());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES)
    public void testVisibilityOnShownConstraints() {
        remakeWithoutSpy();

        // Emit tab event such that we get an active tab observer.
        notifyAddTab(mTab);
        notifyCurrentTab(mTab);

        // Switching tabs locks the controls, advance time past this.
        assertEquals(BrowserControlsState.SHOWN, mControlsDelegate.get().intValue());
        ShadowLooper.idleMainLooper(
                BrowserStateBrowserControlsVisibilityDelegate.MINIMUM_SHOW_DURATION_MS,
                TimeUnit.MILLISECONDS);
        assertEquals(BrowserControlsState.BOTH, mControlsDelegate.get().intValue());

        // Start scrolling to enable optimizations that delay actions.
        notifyContentViewScrollingStateChanged(true);
        assertEquals(View.VISIBLE, mBrowserControlsManager.getAndroidControlsVisibility());

        // Reduce the size of the controls such that we should hide the java view.
        notifyBrowserControlsOffsetChanged(TOOLBAR_HEIGHT, 0);
        assertEquals(View.INVISIBLE, mBrowserControlsManager.getAndroidControlsVisibility());

        // Now scroll the controls back fully onscreen. Suppression layout optimizations should not
        // restore visibility of the java views eagerly.
        notifyBrowserControlsOffsetChanged(0, 0);
        assertEquals(View.INVISIBLE, mBrowserControlsManager.getAndroidControlsVisibility());

        // However when entering SHOWN state, the optimization should ignore scrolling and
        // immediately restore view visibility.
        mControlsDelegate.set(BrowserControlsState.SHOWN);
        assertEquals(View.VISIBLE, mBrowserControlsManager.getAndroidControlsVisibility());
    }

    @Test
    public void testSetControlsPosition() {
        remakeWithoutSpy();
        notifyAddTab(mTab);
        notifyCurrentTab(mTab);

        assertEquals(
                0.0f, mBrowserControlsManager.getBrowserControlHiddenRatio(), MathUtils.EPSILON);

        mBrowserControlsManager.setControlsPosition(
                ControlsPosition.BOTTOM, 0, 0, TOOLBAR_HEIGHT, 10);
        verify(mBrowserControlsStateProviderObserver)
                .onControlsPositionChanged(ControlsPosition.BOTTOM);
        assertEquals(
                0.0f, mBrowserControlsManager.getBrowserControlHiddenRatio(), MathUtils.EPSILON);
        assertEquals(0, mBrowserControlsManager.getTopControlsMinHeight());
        assertEquals(10, mBrowserControlsManager.getBottomControlsMinHeight());

        // Hidden ratio should reflect the bottom offset, not the top.
        notifyBrowserControlsOffsetChanged(TOOLBAR_HEIGHT / 4, TOOLBAR_HEIGHT / 2);
        assertEquals(
                0.5f, mBrowserControlsManager.getBrowserControlHiddenRatio(), MathUtils.EPSILON);

        mBrowserControlsManager.setControlsPosition(ControlsPosition.TOP, TOOLBAR_HEIGHT, 10, 0, 0);
        verify(mBrowserControlsStateProviderObserver)
                .onControlsPositionChanged(ControlsPosition.TOP);
        assertEquals(
                0.25f, mBrowserControlsManager.getBrowserControlHiddenRatio(), MathUtils.EPSILON);
        assertEquals(10, mBrowserControlsManager.getTopControlsMinHeight());
        assertEquals(0, mBrowserControlsManager.getBottomControlsMinHeight());

        // Changing the bottom offset shouldn't affect hidden ratio while position is top.
        notifyBrowserControlsOffsetChanged(TOOLBAR_HEIGHT / 4, TOOLBAR_HEIGHT);
        assertEquals(
                0.25f, mBrowserControlsManager.getBrowserControlHiddenRatio(), MathUtils.EPSILON);
    }

    @Test
    public void testStartWithBottom() {
        BrowserControlsManager browserControlsManager =
                new BrowserControlsManager(
                        mActivity, BrowserControlsStateProvider.ControlsPosition.BOTTOM);
        browserControlsManager.initialize(
                mControlContainer,
                mActivityTabProvider,
                mTabModelSelector,
                R.dimen.control_container_height);
        assertEquals(0, browserControlsManager.getTopControlsHeight());
        assertEquals(TOOLBAR_HEIGHT, browserControlsManager.getBottomControlsHeight());
    }
}
