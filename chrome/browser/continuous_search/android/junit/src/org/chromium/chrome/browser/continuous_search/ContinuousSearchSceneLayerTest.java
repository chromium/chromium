// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.components.browser_ui.widget.ViewResourceFrameLayout;
import org.chromium.ui.resources.ResourceManager;

import java.util.ArrayList;
import java.util.List;

/**
 * Test of {@link ContinuousSearchSceneLayer}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class ContinuousSearchSceneLayerTest {
    private static final long FAKE_NATIVE_ADDRESS = 123L;
    private static final int SHADOW_HEIGHT = 7;
    @Mock
    private ResourceManager mResourceManagerMock;
    @Mock
    private SceneLayer mContentTree;
    @Mock
    private ContinuousSearchSceneLayer.Natives mContinuousSearchSceneLayerJniMock;
    @Mock
    private ViewResourceFrameLayout mContainerView;

    private ContinuousSearchSceneLayer mSceneLayer;

    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Before
    public void setUp() {
        mJniMocker.mock(
                ContinuousSearchSceneLayerJni.TEST_HOOKS, mContinuousSearchSceneLayerJniMock);
        when(mContinuousSearchSceneLayerJniMock.init(any())).thenReturn(FAKE_NATIVE_ADDRESS);

        mSceneLayer =
                new ContinuousSearchSceneLayer(mResourceManagerMock, mContainerView, SHADOW_HEIGHT);

        verify(mContinuousSearchSceneLayerJniMock, times(1)).init(eq(mSceneLayer));
        mSceneLayer.initializeNative();
    }

    /**
     * Tests setting visibility.
     */
    @Test
    public void testVisibility() {
        mSceneLayer.setIsVisible(true);
        Assert.assertTrue(mSceneLayer.isSceneOverlayTreeShowing());

        mSceneLayer.setIsVisible(false);
        Assert.assertFalse(mSceneLayer.isSceneOverlayTreeShowing());
    }

    /**
     * Tests setting and updating native.
     */
    @Test
    public void testSetAndUpdate() {
        InOrder inOrder = inOrder(mContinuousSearchSceneLayerJniMock);
        mSceneLayer.setContentTree(mContentTree);
        inOrder.verify(mContinuousSearchSceneLayerJniMock)
                .setContentTree(eq(FAKE_NATIVE_ADDRESS), eq(mContentTree));

        final int verticalOffset = 5;
        mSceneLayer.setVerticalOffset(verticalOffset);
        final int resourceId = 10;
        mSceneLayer.setResourceId(resourceId);

        mSceneLayer.onSizeChanged(1f, 2f, 3f, 0); // Should have no effect.

        // Show and hide the composited shadow depending on if the Android view is visible.
        when(mContainerView.getVisibility()).thenReturn(View.VISIBLE);
        Assert.assertEquals(
                mSceneLayer, mSceneLayer.getUpdatedSceneOverlayTree(null, null, null, 0f));
        inOrder.verify(mContinuousSearchSceneLayerJniMock)
                .updateContinuousSearchLayer(eq(FAKE_NATIVE_ADDRESS), eq(mResourceManagerMock),
                        eq(resourceId), eq(verticalOffset), eq(false), eq(SHADOW_HEIGHT));

        when(mContainerView.getVisibility()).thenReturn(View.GONE);
        Assert.assertEquals(
                mSceneLayer, mSceneLayer.getUpdatedSceneOverlayTree(null, null, null, 0f));
        inOrder.verify(mContinuousSearchSceneLayerJniMock)
                .updateContinuousSearchLayer(eq(FAKE_NATIVE_ADDRESS), eq(mResourceManagerMock),
                        eq(resourceId), eq(verticalOffset), eq(true), eq(SHADOW_HEIGHT));
    }

    /**
     * Tests the required overrides that default to no-ops.
     */
    @Test
    public void testDefaultBehaviors() {
        Assert.assertNull(mSceneLayer.getEventFilter());

        List<VirtualView> views = new ArrayList<>();
        mSceneLayer.getVirtualViews(views);
        Assert.assertEquals(0, views.size());

        Assert.assertFalse(mSceneLayer.shouldHideAndroidBrowserControls());

        Assert.assertFalse(mSceneLayer.onBackPressed());

        Assert.assertFalse(mSceneLayer.handlesTabCreating());
    }
}
