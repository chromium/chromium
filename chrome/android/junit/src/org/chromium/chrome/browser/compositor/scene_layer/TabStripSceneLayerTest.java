// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package chrome.android.junit.src.org.chromium.chrome.browser.compositor.scene_layer;

import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;
import android.view.ContextThemeWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton;
import org.chromium.chrome.browser.compositor.layouts.components.CompositorButton.ButtonType;
import org.chromium.chrome.browser.compositor.layouts.components.TintedCompositorButton;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutTab;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView.StripLayoutViewOnClickHandler;
import org.chromium.chrome.browser.compositor.overlays.strip.TabLoadTracker.TabLoadTrackerCallback;
import org.chromium.chrome.browser.compositor.scene_layer.TabStripSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.TabStripSceneLayerJni;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.ui.resources.ResourceManager;

/** Tests for {@link TabStripSceneLayer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, qualifiers = "sw600dp")
public class TabStripSceneLayerTest {
    @Rule public JniMocker mJniMocker = new JniMocker();
    @Mock private TabStripSceneLayer.Natives mTabStripSceneMock;
    @Mock private StripLayoutHelperManager mStripLayoutHelperManager;
    @Mock private ResourceManager mResourceManager;
    @Mock private LayerTitleCache mLayerTitleCache;
    @Mock private SceneLayer mSceneLayer;
    @Mock private StripLayoutViewOnClickHandler mOnClickHandler;
    @Mock private TabLoadTrackerCallback mTabLoadTrackerCallback;
    @Mock private LayoutRenderHost mLayoutRenderHost;
    @Mock private LayoutUpdateHost mLayoutUpdateHost;

    private final float mDpToPx = 1.f;

    private CompositorButton mModelSelectorButton;
    private TintedCompositorButton mNewTabButton;
    private Context mContext;
    private TabStripSceneLayer mTabStripSceneLayer;
    private StripLayoutTab mStripLayoutTab;
    private StripLayoutTab[] mStripLayoutTabs;

    public TabStripSceneLayerTest() {}

    @Before
    public void beforeTest() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(TabStripSceneLayerJni.TEST_HOOKS, mTabStripSceneMock);
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        TabStripSceneLayer.setTestFlag(true);
        initializeTest();
    }

    @After
    public void tearDown() {
        TabStripSceneLayer.setTestFlag(false);
    }

    private void initializeTest() {
        mTabStripSceneLayer = new TabStripSceneLayer(mDpToPx);
        when(mTabStripSceneMock.init(mTabStripSceneLayer)).thenReturn(1L);
        mModelSelectorButton =
                new TintedCompositorButton(
                        mContext,
                        ButtonType.INCOGNITO_SWITCHER,
                        null,
                        32.f,
                        32.f,
                        mOnClickHandler,
                        R.drawable.ic_incognito);
        mNewTabButton =
                new TintedCompositorButton(
                        mContext,
                        ButtonType.NEW_TAB,
                        null,
                        32.f,
                        32.f,
                        mOnClickHandler,
                        R.drawable.ic_new_tab_button);
        mStripLayoutTab =
                new StripLayoutTab(
                        mContext,
                        1,
                        mOnClickHandler,
                        mTabLoadTrackerCallback,
                        mLayoutUpdateHost,
                        false);
        mTabStripSceneLayer.initializeNativeForTesting();
        mStripLayoutTabs = new StripLayoutTab[] {mStripLayoutTab};
        when(mStripLayoutHelperManager.getNewTabButton()).thenReturn(mNewTabButton);
        when(mStripLayoutHelperManager.getModelSelectorButton()).thenReturn(mModelSelectorButton);
    }

    @Test
    public void testSetContentTree() {
        mTabStripSceneLayer.setContentTree(mSceneLayer);
        verify(mTabStripSceneMock).setContentTree(1L, mTabStripSceneLayer, mSceneLayer);
    }

    @Test
    public void testPushAndUpdateStrip() {
        float leftPadding = 10f;
        float rightPadding = 20f;
        float topPadding = 5f;
        // Call the method being tested.
        mTabStripSceneLayer.pushAndUpdateStrip(
                mStripLayoutHelperManager,
                mLayerTitleCache,
                mResourceManager,
                mStripLayoutTabs,
                null,
                1.f,
                0,
                -1,
                Color.YELLOW,
                0.3f,
                leftPadding,
                rightPadding,
                topPadding);

        // Verify JNI calls.
        verify(mTabStripSceneMock)
                .updateModelSelectorButton(
                        1L,
                        mTabStripSceneLayer,
                        mModelSelectorButton.getResourceId(),
                        ((TintedCompositorButton) mModelSelectorButton).getBackgroundResourceId(),
                        mModelSelectorButton.getDrawX(),
                        mModelSelectorButton.getDrawY(),
                        true,
                        false,
                        ((TintedCompositorButton) mModelSelectorButton).getTint(),
                        ((TintedCompositorButton) mModelSelectorButton).getBackgroundTint(),
                        mModelSelectorButton.getOpacity(),
                        mResourceManager);
        verify(mTabStripSceneMock)
                .updateNewTabButton(
                        eq(1L),
                        eq(mTabStripSceneLayer),
                        /* resourceId= */ anyInt(),
                        /* backgroundResourceId= */ anyInt(),
                        /* x= */ eq(mNewTabButton.getDrawX() * mDpToPx),
                        /* y= */ eq(mNewTabButton.getDrawY() * mDpToPx),
                        /* topPadding= */ eq(topPadding),
                        /* touchTargetOffset= */ anyFloat(),
                        /* visible= */ eq(mNewTabButton.isVisible()),
                        /* isHovered= */ eq(mNewTabButton.isHovered()),
                        /* tint= */ anyInt(),
                        /* backgroundTint= */ anyInt(),
                        /* buttonAlpha= */ anyFloat(),
                        /* backgroundTint= */ eq(mResourceManager));
        verify(mTabStripSceneMock)
                .updateTabStripLeftFade(
                        1L, mTabStripSceneLayer, 0, 0.f, mResourceManager, 0, leftPadding);
        verify(mTabStripSceneMock)
                .updateTabStripRightFade(
                        1L, mTabStripSceneLayer, 0, 0.f, mResourceManager, 0, rightPadding);
        verify(mTabStripSceneMock)
                .updateTabStripLayer(
                        eq(1L),
                        eq(mTabStripSceneLayer),
                        anyInt(),
                        anyInt(),
                        /* yOffset= */ eq(1.f),
                        anyInt(),
                        /* scrimColor= */ eq(Color.YELLOW),
                        /* scrimOpacity= */ eq(0.3f),
                        eq(leftPadding),
                        eq(rightPadding),
                        eq(topPadding));
    }
}
