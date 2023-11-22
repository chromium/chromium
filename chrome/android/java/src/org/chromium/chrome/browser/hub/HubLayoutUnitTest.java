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
import static org.mockito.ArgumentMatchers.isNotNull;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.ui.test.util.MockitoHelper.doCallback;

import android.animation.AnimatorSet;
import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.RectF;
import android.view.ViewGroup;
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

import org.chromium.base.Callback;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.LazyOneshotSupplierImpl;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.SyncOneshotSupplierImpl;
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
import org.chromium.chrome.browser.tab.TabHidingType;
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

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private LayoutUpdateHost mUpdateHost;
    @Mock private LayoutRenderHost mRenderHost;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private ResourceManager mResourceManager;
    @Mock private SceneLayer.Natives mSceneLayerJni;
    @Mock private StaticTabSceneLayer.Natives mStaticTabSceneLayerJni;
    @Mock private HubManager mHubManager;
    @Mock private HubController mHubController;
    @Mock private PaneManager mPaneManager;
    @Mock private HubLayoutScrimController mScrimController;
    @Mock private Pane mPane;
    @Mock private HubLayoutAnimator mHubLayoutAnimatorMock;
    @Mock private HubLayoutAnimatorProvider mHubLayoutAnimatorProviderMock;
    @Mock private Bitmap mBitmap;
    @Mock private Callback<Bitmap> mThumbnailCallback;
    @Mock private TabContentManager mTabContentManager;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mTab;

    private Activity mActivity;
    private FrameLayout mFrameLayout;

    private HubLayout mHubLayout;
    private HubContainerView mHubContainerView;

    private SyncOneshotSupplierImpl<HubLayoutAnimator> mHubLayoutAnimatorSupplier;
    private ObservableSupplierImpl<Pane> mPaneSupplier;

    @Before
    public void setUp() {
        mJniMocker.mock(SceneLayerJni.TEST_HOOKS, mSceneLayerJni);
        mJniMocker.mock(StaticTabSceneLayerJni.TEST_HOOKS, mStaticTabSceneLayerJni);

        when(mSceneLayerJni.init(any()))
                .thenReturn(FAKE_NATIVE_ADDRESS_1)
                .thenReturn(FAKE_NATIVE_ADDRESS_2);
        // Fake proper cleanup of the native ptr.
        doCallback(
                        /* index= */ 1,
                        (SceneLayer sceneLayer) -> {
                            sceneLayer.setNativePtr(0L);
                        })
                .when(mSceneLayerJni)
                .destroy(anyLong(), any());
        // Ensure SceneLayer has a native ptr.
        doAnswer(
                        invocation -> {
                            ((SceneLayer) invocation.getArguments()[0])
                                    .setNativePtr(FAKE_NATIVE_ADDRESS_1);
                            return FAKE_NATIVE_ADDRESS_1;
                        })
                .when(mStaticTabSceneLayerJni)
                .init(any());

        when(mHubManager.getPaneManager()).thenReturn(mPaneManager);
        when(mHubManager.getHubController()).thenReturn(mHubController);
        LazyOneshotSupplier<HubManager> hubManagerSupplier =
                new LazyOneshotSupplierImpl<HubManager>() {
                    @Override
                    public void doSet() {
                        set(mHubManager);
                    }
                };
        LazyOneshotSupplier<ViewGroup> rootViewSupplier =
                new LazyOneshotSupplierImpl<ViewGroup>() {
                    @Override
                    public void doSet() {
                        set(mFrameLayout);
                    }
                };

        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) -> {
                            mActivity = activity;
                            mFrameLayout = new FrameLayout(mActivity);
                            mHubContainerView = new HubContainerView(mActivity);
                            mActivity.setContentView(mFrameLayout);

                            when(mHubController.getContainerView()).thenReturn(mHubContainerView);

                            HubLayoutDependencyHolder dependencyHolder =
                                    new HubLayoutDependencyHolder(
                                            hubManagerSupplier, rootViewSupplier, mScrimController);

                            mHubLayout =
                                    spy(
                                            new HubLayout(
                                                    mActivity,
                                                    mUpdateHost,
                                                    mRenderHost,
                                                    mLayoutStateProvider,
                                                    dependencyHolder));
                            mHubLayout.setTabModelSelector(mTabModelSelector);
                            mHubLayout.setTabContentManager(mTabContentManager);
                            mHubLayout.onFinishNativeInitialization();
                        });

        doAnswer(
                        invocation -> {
                            var args = invocation.getArguments();
                            return new LayoutTab(
                                    (Integer) args[0],
                                    (Boolean) args[1],
                                    ((Float) args[2]).intValue(),
                                    ((Float) args[3]).intValue());
                        })
                .when(mUpdateHost)
                .createLayoutTab(anyInt(), anyBoolean(), anyFloat(), anyFloat());
        when(mTab.getId()).thenReturn(TAB_ID);
        when(mTab.isNativePage()).thenReturn(false);
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);

        mHubLayoutAnimatorSupplier = new SyncOneshotSupplierImpl<HubLayoutAnimator>();
        when(mHubLayoutAnimatorProviderMock.getAnimatorSupplier())
                .thenReturn(mHubLayoutAnimatorSupplier);

        mPaneSupplier = new ObservableSupplierImpl<Pane>();
        when(mPaneManager.getFocusedPaneSupplier()).thenReturn(mPaneSupplier);
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
        mHubLayout.updateSceneLayer(
                new RectF(),
                new RectF(),
                mTabContentManager,
                mResourceManager,
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
        verify(mTabContentManager).cacheTabThumbnail(any());
        verify(mTabContentManager, never())
                .cacheTabThumbnailWithCallback(any(), anyBoolean(), any());
    }

    @Test
    @SmallTest
    public void testShowFromStartSurface() {
        mPaneSupplier.set(mPane);
        show(LayoutType.START_SURFACE, true, HubLayoutAnimationType.FADE_IN);
        verify(mTabContentManager, never()).cacheTabThumbnail(any());
        verify(mTabContentManager, never())
                .cacheTabThumbnailWithCallback(any(), anyBoolean(), any());
        verify(mPane, never()).createShowHubLayoutAnimatorProvider(any());
    }

    @Test
    @SmallTest
    public void testShowWithNoPane() {
        show(LayoutType.BROWSING, true, HubLayoutAnimationType.FADE_IN);
        verify(mTabContentManager).cacheTabThumbnail(any());
    }

    @Test
    @SmallTest
    public void testShowFromBrowsingWithThumbnailCallback() {
        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.SHRINK_TAB);
        when(mHubLayoutAnimatorProviderMock.getThumbnailCallback()).thenReturn(mThumbnailCallback);
        mPaneSupplier.set(mPane);
        when(mPane.createShowHubLayoutAnimatorProvider(any()))
                .thenReturn(mHubLayoutAnimatorProviderMock);

        // Successfully capture a bitmap.
        doCallback(
                        /* index= */ 2,
                        (Callback<Bitmap> bitmapCallback) -> {
                            bitmapCallback.onResult(mBitmap);
                        })
                .when(mTabContentManager)
                .cacheTabThumbnailWithCallback(any(), eq(true), any());

        show(LayoutType.BROWSING, true, HubLayoutAnimationType.SHRINK_TAB);

        verify(mThumbnailCallback).onResult(isNotNull());
        verify(mTabContentManager, never()).cacheTabThumbnail(any());
    }

    @Test
    @SmallTest
    public void testShowFromBrowsingWithFallbackNativePageThumbnailCallback() {
        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.SHRINK_TAB);
        when(mHubLayoutAnimatorProviderMock.getThumbnailCallback()).thenReturn(mThumbnailCallback);
        mPaneSupplier.set(mPane);
        when(mPane.createShowHubLayoutAnimatorProvider(any()))
                .thenReturn(mHubLayoutAnimatorProviderMock);
        when(mTab.isNativePage()).thenReturn(true);

        // Fail to capture a bitmap.
        doCallback(
                        /* index= */ 2,
                        (Callback<Bitmap> bitmapCallback) -> {
                            bitmapCallback.onResult(null);
                        })
                .when(mTabContentManager)
                .cacheTabThumbnailWithCallback(any(), eq(true), any());

        // Succeed on the NativePage fallback thumbnail attempt.
        doCallback(
                        /* index= */ 1,
                        (Callback<Bitmap> bitmapCallback) -> {
                            bitmapCallback.onResult(mBitmap);
                        })
                .when(mTabContentManager)
                .getEtc1TabThumbnailWithCallback(eq(TAB_ID), any());

        show(LayoutType.BROWSING, true, HubLayoutAnimationType.SHRINK_TAB);

        verify(mThumbnailCallback).onResult(isNotNull());
        verify(mTabContentManager, never()).cacheTabThumbnail(any());
    }

    @Test
    @SmallTest
    public void testShowFromBrowsingWithoutFallbackThumbnailCallback() {
        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.SHRINK_TAB);
        when(mHubLayoutAnimatorProviderMock.getThumbnailCallback()).thenReturn(mThumbnailCallback);
        mPaneSupplier.set(mPane);
        when(mPane.createShowHubLayoutAnimatorProvider(any()))
                .thenReturn(mHubLayoutAnimatorProviderMock);

        // Fail to capture the bitmap and since this is not a native page there is no fallback.
        doCallback(
                        /* index= */ 2,
                        (Callback<Bitmap> bitmapCallback) -> {
                            bitmapCallback.onResult(null);
                        })
                .when(mTabContentManager)
                .cacheTabThumbnailWithCallback(any(), eq(true), any());

        show(LayoutType.BROWSING, true, HubLayoutAnimationType.SHRINK_TAB);

        verify(mThumbnailCallback).onResult(isNull());
        verify(mTabContentManager, never()).getEtc1TabThumbnailWithCallback(anyInt(), any());
        verify(mTabContentManager, never()).cacheTabThumbnail(any());
    }

    @Test
    @SmallTest
    public void testShowFromStartSurfaceWithThumbnailCallback() {
        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.SHRINK_TAB);
        when(mHubLayoutAnimatorProviderMock.getThumbnailCallback()).thenReturn(mThumbnailCallback);
        doReturn(mHubLayoutAnimatorProviderMock).when(mHubLayout).createShowAnimatorProvider(any());

        show(LayoutType.START_SURFACE, true, HubLayoutAnimationType.SHRINK_TAB);

        // No TabContentManager callbacks will be invoked because there is no tab to capture.
        // This will still invoke the callback with a null result.
        verify(mThumbnailCallback).onResult(isNull());
        verify(mTabContentManager, never()).cacheTabThumbnail(any());
        verify(mTabContentManager, never())
                .cacheTabThumbnailWithCallback(any(), anyBoolean(), any());
        verify(mTabContentManager, never()).getEtc1TabThumbnailWithCallback(anyInt(), any());
    }

    @Test
    @SmallTest
    @Config(qualifiers = "sw600dp")
    public void testHideTablet() {
        hide(LayoutType.BROWSING, TAB_ID, HubLayoutAnimationType.TRANSLATE_DOWN);
        verify(mTabContentManager, never()).getEtc1TabThumbnailWithCallback(anyInt(), any());
    }

    @Test
    @SmallTest
    public void testHideToStartSurface() {
        mPaneSupplier.set(mPane);
        hide(LayoutType.START_SURFACE, Tab.INVALID_TAB_ID, HubLayoutAnimationType.FADE_OUT);
        verify(mTabContentManager, never()).getEtc1TabThumbnailWithCallback(anyInt(), any());
        verify(mPane, never()).createHideHubLayoutAnimatorProvider(any());
    }

    @Test
    @SmallTest
    public void testHideWithNoPane() {
        hide(LayoutType.BROWSING, Tab.INVALID_TAB_ID, HubLayoutAnimationType.FADE_OUT);
        verify(mTabContentManager, never()).getEtc1TabThumbnailWithCallback(anyInt(), any());
    }

    @Test
    @SmallTest
    public void testHideViaNewTab() {
        mHubLayout.onTabCreated(FAKE_TIME, NEW_TAB_ID, NEW_TAB_INDEX, TAB_ID, false, false, 0, 0);
        hide(LayoutType.BROWSING, NEW_TAB_ID, HubLayoutAnimationType.NEW_TAB);
        verify(mTabContentManager, never()).getEtc1TabThumbnailWithCallback(anyInt(), any());
    }

    @Test
    @SmallTest
    public void testHideToBrowsingThumbnailCallback() {
        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.EXPAND_TAB);
        when(mHubLayoutAnimatorProviderMock.getThumbnailCallback()).thenReturn(mThumbnailCallback);
        mPaneSupplier.set(mPane);
        when(mPane.createHideHubLayoutAnimatorProvider(any()))
                .thenReturn(mHubLayoutAnimatorProviderMock);
        when(mTab.isNativePage()).thenReturn(true);

        // Succeed on the thumbnail attempt
        doCallback(
                        /* index= */ 1,
                        (Callback<Bitmap> bitmapCallback) -> {
                            bitmapCallback.onResult(mBitmap);
                        })
                .when(mTabContentManager)
                .getEtc1TabThumbnailWithCallback(eq(TAB_ID), any());

        hide(LayoutType.BROWSING, TAB_ID, HubLayoutAnimationType.EXPAND_TAB);

        verify(mThumbnailCallback).onResult(isNotNull());
    }

    @Test
    @SmallTest
    public void testHideToBrowsingThumbnailCallbackNoTabIdInStartHiding() {
        when(mTabModelSelector.getCurrentTabId()).thenReturn(TAB_ID);

        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.EXPAND_TAB);
        when(mHubLayoutAnimatorProviderMock.getThumbnailCallback()).thenReturn(mThumbnailCallback);
        doReturn(mHubLayoutAnimatorProviderMock)
                .when(mHubLayout)
                .createHideAnimatorProvider(any(), anyInt());
        when(mTab.isNativePage()).thenReturn(true);

        // Succeed on the thumbnail attempt
        doCallback(
                        /* index= */ 1,
                        (Callback<Bitmap> bitmapCallback) -> {
                            bitmapCallback.onResult(mBitmap);
                        })
                .when(mTabContentManager)
                .getEtc1TabThumbnailWithCallback(eq(TAB_ID), any());

        hide(LayoutType.BROWSING, Tab.INVALID_TAB_ID, HubLayoutAnimationType.EXPAND_TAB);

        verify(mThumbnailCallback).onResult(isNotNull());
    }

    @Test
    @SmallTest
    public void testHideToStartSurfaceThumbnailCallback() {
        setupHubLayoutAnimatorAndProvider(HubLayoutAnimationType.EXPAND_TAB);
        when(mHubLayoutAnimatorProviderMock.getThumbnailCallback()).thenReturn(mThumbnailCallback);
        doReturn(mHubLayoutAnimatorProviderMock)
                .when(mHubLayout)
                .createHideAnimatorProvider(any(), anyInt());
        when(mTab.isNativePage()).thenReturn(true);

        hide(LayoutType.START_SURFACE, TAB_ID, HubLayoutAnimationType.EXPAND_TAB);

        verify(mThumbnailCallback).onResult(isNull());
        verify(mTabContentManager, never()).getEtc1TabThumbnailWithCallback(anyInt(), any());
    }

    @Test
    @SmallTest
    public void testShowInterruptedByHide() {
        assertFalse(mHubLayout.isRunningAnimations());
        assertFalse(mHubLayout.onUpdateAnimation(FAKE_TIME, false));

        startShowing(LayoutType.BROWSING, true);

        verify(mHubController, times(1)).onHubLayoutShow();
        assertEquals(1, mFrameLayout.getChildCount());

        assertEquals(HubLayoutAnimationType.FADE_IN, mHubLayout.getCurrentAnimationType());
        assertTrue(mHubLayout.isRunningAnimations());
        assertTrue(mHubLayout.onUpdateAnimation(FAKE_TIME, false));

        startHiding(LayoutType.BROWSING, NEW_TAB_ID, false);
        verify(mHubLayout).doneShowing();
        verify(mTab, never()).hide(anyInt());
        verify(mScrimController).forceAnimationToFinish();

        assertEquals(HubLayoutAnimationType.FADE_OUT, mHubLayout.getCurrentAnimationType());
        assertTrue(mHubLayout.isRunningAnimations());
        assertTrue(mHubLayout.onUpdateAnimation(FAKE_TIME, false));

        ShadowLooper.runUiThreadTasks();

        assertFalse(mHubLayout.isRunningAnimations());
        assertFalse(mHubLayout.onUpdateAnimation(FAKE_TIME, false));

        verify(mHubController, times(1)).onHubLayoutDoneHiding();
        assertEquals(0, mFrameLayout.getChildCount());
        verify(mHubLayout).doneHiding();
        verify(mTab, never()).hide(anyInt());
    }

    private void show(
            @LayoutType int fromLayout,
            boolean animate,
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
        verify(mHubLayout).doneShowing();
        verify(mTab).hide(eq(TabHidingType.TAB_SWITCHER_SHOWN));
        verify(mScrimController, never()).forceAnimationToFinish();
    }

    private void hide(
            @LayoutType int nextLayout,
            int nextTabId,
            @HubLayoutAnimationType int expectedAnimationType) {
        if (expectedAnimationType == HubLayoutAnimationType.NEW_TAB) {
            assertTrue(mHubLayout.isRunningAnimations());
            assertTrue(mHubLayout.onUpdateAnimation(FAKE_TIME, false));
        } else {
            assertFalse(mHubLayout.isRunningAnimations());
            assertFalse(mHubLayout.onUpdateAnimation(FAKE_TIME, false));
        }

        startHiding(nextLayout, nextTabId, true);

        assertEquals(expectedAnimationType, mHubLayout.getCurrentAnimationType());
        assertTrue(mHubLayout.isRunningAnimations());
        assertTrue(mHubLayout.onUpdateAnimation(FAKE_TIME, false));

        ShadowLooper.runUiThreadTasks();

        assertFalse(mHubLayout.isRunningAnimations());
        assertFalse(mHubLayout.onUpdateAnimation(FAKE_TIME, false));

        verify(mHubController, times(1)).onHubLayoutDoneHiding();
        assertEquals(0, mFrameLayout.getChildCount());
        verify(mHubLayout).doneHiding();
        verify(mScrimController, never()).forceAnimationToFinish();
    }

    private void startShowing(@LayoutType int fromLayout, boolean animate) {
        when(mLayoutStateProvider.getActiveLayoutType()).thenReturn(fromLayout);
        mHubLayout.contextChanged(mActivity);

        mHubLayout.show(FAKE_TIME, animate);
    }

    private void startHiding(
            @LayoutType int nextLayout, int nextTabId, boolean hintAtTabSelection) {
        @LayoutType int layoutType = mHubLayout.getLayoutType();
        when(mLayoutStateProvider.getActiveLayoutType()).thenReturn(layoutType);
        when(mLayoutStateProvider.getNextLayoutType()).thenReturn(nextLayout);

        mHubLayout.startHiding(nextTabId, hintAtTabSelection);
    }

    private void setupHubLayoutAnimatorAndProvider(@HubLayoutAnimationType int animationType) {
        AnimatorSet animatorSet = new AnimatorSet();
        when(mHubLayoutAnimatorMock.getAnimationType()).thenReturn(animationType);
        when(mHubLayoutAnimatorMock.getAnimatorSet()).thenReturn(animatorSet);
        when(mHubLayoutAnimatorProviderMock.getPlannedAnimationType()).thenReturn(animationType);
        mHubLayoutAnimatorSupplier.set(mHubLayoutAnimatorMock);
    }
}
