// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.hamcrest.Matchers.instanceOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.app.Activity;
import android.graphics.RectF;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout.ViewportMode;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayerJni;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayerJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.resources.ResourceManager;

import java.util.Collections;

/**
 * Unit tests for {@link HubLayout}.
 *
 * <p>TODO(crbug/1487209): Once integrated with LayoutManager we should also add integration tests.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HubLayoutUnitTest {
    private static final long FAKE_NATIVE_ADDRESS_1 = 498723734L;
    private static final long FAKE_NATIVE_ADDRESS_2 = 123210L;
    private static final float FLOAT_ERROR = 0.001f;
    private static final int TAB_ID = 5;
    private static final int NEW_TAB_ID = 6;
    private static final int NEW_TAB_INDEX = 0;
    // This animation doesn't depend on time from the LayoutManager.
    private static final long FAKE_TIME = 0L;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock
    private LayoutUpdateHost mUpdateHost;
    @Mock
    private LayoutRenderHost mRenderHost;
    @Mock
    private LayoutStateProvider mLayoutStateProvider;
    @Mock private HubController mHubController;
    @Mock private Tab mTab;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private TabContentManager mTabContentManager;
    @Mock
    private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock
    private ResourceManager mResourceManager;
    @Mock
    private SceneLayer.Natives mSceneLayerJni;
    @Mock
    private StaticTabSceneLayer.Natives mStaticTabSceneLayerJni;

    private Activity mActivity;
    private FrameLayout mFrameLayout;

    private HubLayout mHubLayout;
    private HubContainerView mHubContainerView;

    @Before
    public void setUp() {
        mJniMocker.mock(SceneLayerJni.TEST_HOOKS, mSceneLayerJni);
        mJniMocker.mock(StaticTabSceneLayerJni.TEST_HOOKS, mStaticTabSceneLayerJni);

        when(mSceneLayerJni.init(any()))
                .thenReturn(FAKE_NATIVE_ADDRESS_1)
                .thenReturn(FAKE_NATIVE_ADDRESS_2);
        // Fake proper cleanup of the native ptr.
        doCallback(/*index=*/1, (SceneLayer sceneLayer) -> {
            sceneLayer.setNativePtr(0L);
        }).when(mSceneLayerJni).destroy(anyLong(), any());
        // Ensure SceneLayer has a native ptr.
        doAnswer(invocation -> {
            ((SceneLayer) invocation.getArguments()[0]).setNativePtr(FAKE_NATIVE_ADDRESS_1);
            return FAKE_NATIVE_ADDRESS_1;
        })
                .when(mStaticTabSceneLayerJni)
                .init(any());

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mActivity = activity;
                            mFrameLayout = new FrameLayout(mActivity);
                            mHubContainerView = new HubContainerView(mActivity);
                            mActivity.setContentView(mFrameLayout);

                            when(mHubController.getContainerView()).thenReturn(mHubContainerView);

                            mHubLayout =
                                    new HubLayout(
                                            mActivity,
                                            mUpdateHost,
                                            mRenderHost,
                                            mLayoutStateProvider,
                                            mFrameLayout,
                                            mHubController);
                            mHubLayout.setTabModelSelector(mTabModelSelector);
                            mHubLayout.setTabContentManager(mTabContentManager);
                            mHubLayout.onFinishNativeInitialization();
                        });

        doAnswer(invocation -> {
            var args = invocation.getArguments();
            return new LayoutTab((Integer) args[0], (Boolean) args[1], ((Float) args[2]).intValue(),
                    ((Float) args[3]).intValue());
        })
                .when(mUpdateHost)
                .createLayoutTab(anyInt(), anyBoolean(), anyFloat(), anyFloat());
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
    }

    @After
    public void tearDown() {
        mHubLayout.destroy();
    }

    @Test
    @SmallTest
    public void testFixedReturnValues() {
        // These are not expected to change. This is here to get unit test coverage.
        assertEquals(ViewportMode.ALWAYS_FULLSCREEN, mHubLayout.getViewportMode());
        assertTrue(mHubLayout.handlesTabClosing());
        assertTrue(mHubLayout.handlesTabCreating());
        assertNull(mHubLayout.getEventFilter());
        assertEquals(LayoutType.TAB_SWITCHER, mHubLayout.getLayoutType());

        // TODO(crbug/1487209): These may be dynamic after further development.
        assertFalse(mHubLayout.onBackPressed());
        assertTrue(mHubLayout.canHostBeFocusable());
    }

    @Test
    @SmallTest
    public void testUpdateLayoutAndLayoutTabsDuringShow() {
        assertThat(mHubLayout.getSceneLayer(), instanceOf(SceneLayer.class));
        LayoutTab[] layoutTabs = mHubLayout.getLayoutTabsToRender();
        assertNull(layoutTabs);

        mHubLayout.updateLayout(FAKE_TIME, FAKE_TIME);
        verify(mUpdateHost, never()).requestUpdate();

        startShowing(LayoutType.BROWSING, true);

        assertThat(mHubLayout.getSceneLayer(), instanceOf(StaticTabSceneLayer.class));
        layoutTabs = mHubLayout.getLayoutTabsToRender();
        assertEquals(1, layoutTabs.length);
        assertEquals(TAB_ID, layoutTabs[0].getId());
        verify(mTabContentManager, times(1))
                .updateVisibleIds(eq(Collections.singletonList(TAB_ID)), eq(Tab.INVALID_TAB_ID));

        assertEquals(0f, layoutTabs[0].get(LayoutTab.CONTENT_OFFSET), FLOAT_ERROR);

        float contentOffset = 100f;
        when(mBrowserControlsStateProvider.getContentOffset())
                .thenReturn(Math.round(contentOffset));
        mHubLayout.updateSceneLayer(new RectF(), new RectF(), mTabContentManager, mResourceManager,
                mBrowserControlsStateProvider);
        assertEquals(contentOffset, layoutTabs[0].get(LayoutTab.CONTENT_OFFSET), FLOAT_ERROR);

        // Change this so updateSnap() returns true.
        layoutTabs[0].set(LayoutTab.RENDER_X, 5);
        mHubLayout.updateLayout(FAKE_TIME, FAKE_TIME);
        verify(mUpdateHost, times(1)).requestUpdate();

        ShadowLooper.runUiThreadTasks();

        assertThat(mHubLayout.getSceneLayer(), instanceOf(SceneLayer.class));
        layoutTabs = mHubLayout.getLayoutTabsToRender();
        assertNull(layoutTabs);
        verify(mTabContentManager, times(1))
                .updateVisibleIds(eq(Collections.emptyList()), eq(Tab.INVALID_TAB_ID));
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testShowTablet() {
        show(LayoutType.BROWSING, true, HubLayoutAnimationType.TRANSLATE_UP);
    }

    @Test
    @SmallTest
    public void testShowFromBrowsing() {
        show(LayoutType.BROWSING, true, HubLayoutAnimationType.SHRINK_TAB);
    }

    @Test
    @SmallTest
    public void testShowFromStartSurface() {
        show(LayoutType.START_SURFACE, true, HubLayoutAnimationType.FADE_IN);
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testHideTablet() {
        hide(LayoutType.BROWSING, TAB_ID, true, HubLayoutAnimationType.TRANSLATE_DOWN);
    }

    @Test
    @SmallTest
    public void testHideToBrowsing() {
        hide(LayoutType.BROWSING, TAB_ID, true, HubLayoutAnimationType.EXPAND_TAB);
    }

    @Test
    @SmallTest
    public void testHideToStartSurface() {
        hide(LayoutType.START_SURFACE, Tab.INVALID_TAB_ID, false, HubLayoutAnimationType.FADE_OUT);
    }

    @Test
    @SmallTest
    public void testHideViaNewTab() {
        mHubLayout.onTabCreated(FAKE_TIME, NEW_TAB_ID, NEW_TAB_INDEX, TAB_ID, false, false, 0, 0);
        hide(LayoutType.BROWSING, NEW_TAB_ID, false, HubLayoutAnimationType.NEW_TAB);
    }

    @Test
    @SmallTest
    public void testShowInterruptedByHide() {
        assertFalse(mHubLayout.isRunningAnimations());
        assertFalse(mHubLayout.onUpdateAnimation(FAKE_TIME, false));

        startShowing(LayoutType.BROWSING, true);

        verify(mHubController, times(1)).onHubLayoutShow();
        assertEquals(1, mFrameLayout.getChildCount());

        assertEquals(HubLayoutAnimationType.SHRINK_TAB, mHubLayout.getCurrentAnimationType());
        assertTrue(mHubLayout.isRunningAnimations());
        assertTrue(mHubLayout.onUpdateAnimation(FAKE_TIME, false));

        startHiding(LayoutType.BROWSING, NEW_TAB_ID, false);

        assertEquals(HubLayoutAnimationType.EXPAND_TAB, mHubLayout.getCurrentAnimationType());
        assertTrue(mHubLayout.isRunningAnimations());
        assertTrue(mHubLayout.onUpdateAnimation(FAKE_TIME, false));

        ShadowLooper.runUiThreadTasks();

        assertFalse(mHubLayout.isRunningAnimations());
        assertFalse(mHubLayout.onUpdateAnimation(FAKE_TIME, false));

        verify(mHubController, times(1)).onHubLayoutDoneHiding();
        assertEquals(0, mFrameLayout.getChildCount());
    }

    private void show(@LayoutType int fromLayout, boolean animate,
            @HubLayoutAnimationType int expectedAnimationType) {
        assertFalse(mHubLayout.isRunningAnimations());
        assertFalse(mHubLayout.onUpdateAnimation(FAKE_TIME, false));

        startShowing(fromLayout, animate);

        verify(mHubController, times(1)).onHubLayoutShow();
        assertEquals(1, mFrameLayout.getChildCount());

        if (animate) {
            assertEquals(expectedAnimationType, mHubLayout.getCurrentAnimationType());
            assertTrue(mHubLayout.isRunningAnimations());
            assertTrue(mHubLayout.onUpdateAnimation(FAKE_TIME, false));
        }

        ShadowLooper.runUiThreadTasks();

        assertFalse(mHubLayout.isRunningAnimations());
        assertFalse(mHubLayout.onUpdateAnimation(FAKE_TIME, false));
    }

    private void hide(@LayoutType int nextLayout, int nextTabId, boolean hintAtTabSelection,
            @HubLayoutAnimationType int expectedAnimationType) {
        if (expectedAnimationType == HubLayoutAnimationType.NEW_TAB) {
            assertTrue(mHubLayout.isRunningAnimations());
            assertTrue(mHubLayout.onUpdateAnimation(FAKE_TIME, false));
        } else {
            assertFalse(mHubLayout.isRunningAnimations());
            assertFalse(mHubLayout.onUpdateAnimation(FAKE_TIME, false));
        }

        startHiding(nextLayout, nextTabId, hintAtTabSelection);

        assertEquals(expectedAnimationType, mHubLayout.getCurrentAnimationType());
        assertTrue(mHubLayout.isRunningAnimations());
        assertTrue(mHubLayout.onUpdateAnimation(FAKE_TIME, false));

        ShadowLooper.runUiThreadTasks();

        assertFalse(mHubLayout.isRunningAnimations());
        assertFalse(mHubLayout.onUpdateAnimation(FAKE_TIME, false));

        verify(mHubController, times(1)).onHubLayoutDoneHiding();
        assertEquals(0, mFrameLayout.getChildCount());
    }

    private void startShowing(@LayoutType int fromLayout, boolean animate) {
        when(mLayoutStateProvider.getActiveLayoutType()).thenReturn(fromLayout);
        mHubLayout.contextChanged(mActivity);

        mHubLayout.show(FAKE_TIME, animate);
    }

    private void startHiding(
            @LayoutType int nextLayout, int nextTabId, boolean hintAtTabSelection) {
        when(mLayoutStateProvider.getActiveLayoutType()).thenReturn(mHubLayout.getLayoutType());
        when(mLayoutStateProvider.getNextLayoutType()).thenReturn(nextLayout);

        mHubLayout.startHiding(nextTabId, hintAtTabSelection);
    }
}
