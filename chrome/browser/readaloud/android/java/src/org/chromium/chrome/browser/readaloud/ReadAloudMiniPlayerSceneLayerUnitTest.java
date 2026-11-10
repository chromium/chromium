// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

import android.graphics.RectF;

import org.jni_zero.JniRawPtr;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayerJni;

/** Unit tests for {ReadAloudMiniPlayerSceneLayer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ReadAloudMiniPlayerSceneLayerUnitTest {
    private static final RectF VIEWPORT = new RectF(0f, 0f, 500f, 1000f);
    private static final long PTR = 123456789L;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private SceneLayer.Natives mBaseSceneLayerJni;
    @Mock private ReadAloudMiniPlayerSceneLayerJni mSceneLayerJni;

    private JniRawPtr<ReadAloudMiniPlayerSceneLayer.NativeReadAloudMiniPlayerSceneLayer> mNativePtr;
    private ReadAloudMiniPlayerSceneLayer mSceneLayer;

    @Before
    public void setUp() {
        SceneLayerJni.setInstanceForTesting(mBaseSceneLayerJni);
        ReadAloudMiniPlayerSceneLayerJni.setInstanceForTesting(mSceneLayerJni);
        doAnswer(
                        invocation -> {
                            SceneLayer layer = invocation.getArgument(0);
                            layer.setNativePtr(PTR);
                            mNativePtr = JniRawPtr.createForTesting(PTR);
                            return mNativePtr;
                        })
                .when(mSceneLayerJni)
                .init(any());
        doAnswer(
                        invocation -> {
                            mSceneLayer.setNativePtr(0L);
                            return null;
                        })
                .when(mBaseSceneLayerJni)
                .destroy(anyLong());
        mSceneLayer = new ReadAloudMiniPlayerSceneLayer(mBrowserControlsStateProvider);
    }

    @After
    public void tearDown() {
        mSceneLayer.destroy();
    }

    @Test
    public void testSetSize() {
        final int layerHeight = 100;
        final int bottomControlsMinHeightOffset = 25;
        doReturn(bottomControlsMinHeightOffset)
                .when(mBrowserControlsStateProvider)
                .getBottomControlsMinHeightOffset();

        mSceneLayer.setSize((int) VIEWPORT.width(), (int) layerHeight);
        mSceneLayer.getUpdatedSceneOverlayTree(VIEWPORT, VIEWPORT, /* resourceManager= */ null);

        verify(mSceneLayerJni)
                .updateReadAloudMiniPlayerLayer(
                        eq(mNativePtr),
                        /* colorArgb= */ anyInt(),
                        /* width= */ eq((int) VIEWPORT.width()),
                        /* viewportHeight= */ eq((int) VIEWPORT.height()),
                        /* containerHeight= */ eq(layerHeight),
                        /* bottomOffset= */ eq(bottomControlsMinHeightOffset));
    }

    @Test
    public void testInitAndDestroy() {
        verify(mSceneLayerJni).init(eq(mSceneLayer));
        mSceneLayer.setIsVisible(true);
        assertTrue(mSceneLayer.isSceneOverlayTreeShowing());
        mSceneLayer.destroy();
        verify(mBaseSceneLayerJni).destroy(eq(PTR));
        assertFalse(mSceneLayer.isSceneOverlayTreeShowing());
        assertNull(
                mSceneLayer.getUpdatedSceneOverlayTree(
                        VIEWPORT, VIEWPORT, /* resourceManager= */ null));
        mSceneLayer.setContentTree(Mockito.mock(SceneLayer.class));
        Mockito.verifyNoMoreInteractions(mSceneLayerJni);
    }

    @Test
    public void testSetColor() {
        mSceneLayer.setColor(0xAABBCCFF);
        mSceneLayer.getUpdatedSceneOverlayTree(VIEWPORT, VIEWPORT, /* resourceManager= */ null);

        verify(mSceneLayerJni)
                .updateReadAloudMiniPlayerLayer(
                        eq(mNativePtr),
                        /* colorArgb= */ eq(0xAABBCCFF),
                        /* width= */ anyInt(),
                        /* viewportHeight= */ anyInt(),
                        /* containerHeight= */ anyInt(),
                        /* bottomOffset= */ anyInt());
    }

    @Test
    public void testSetVisibility() {
        assertFalse(mSceneLayer.isSceneOverlayTreeShowing());
        mSceneLayer.setIsVisible(true);
        assertTrue(mSceneLayer.isSceneOverlayTreeShowing());
    }

    @Test
    public void testSetContentTree() {
        SceneLayer contentTree = Mockito.mock(SceneLayer.class);
        mSceneLayer.setContentTree(contentTree);

        verify(mSceneLayerJni).setContentTree(eq(mNativePtr), eq(contentTree));
    }

    @Test
    public void testMiscProperties() {
        assertNull(mSceneLayer.getEventFilter());
        assertFalse(mSceneLayer.shouldHideAndroidBrowserControls());
        assertFalse(mSceneLayer.updateOverlay(0L, 0L));
        assertFalse(mSceneLayer.onBackPressed());
        assertFalse(mSceneLayer.handlesTabCreating());
    }
}
