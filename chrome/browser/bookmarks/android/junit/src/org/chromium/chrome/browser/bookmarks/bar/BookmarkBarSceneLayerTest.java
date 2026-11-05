// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

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
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayerJni;
import org.chromium.ui.resources.ResourceManager;

/** Unit tests for {BookmarkBarSceneLayer}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BookmarkBarSceneLayerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private SceneLayer.Natives mBaseSceneLayerJni;
    @Mock private BookmarkBarSceneLayerJni mSceneLayerJni;
    @Mock private ResourceManager mResourceManager;

    private static final long NATIVE_PTR = 123L;
    private JniRawPtr<BookmarkBarSceneLayer.NativeBookmarkBarSceneLayer> mNativePtr;
    private BookmarkBarSceneLayer mSceneLayer;

    @Before
    public void setUp() {
        SceneLayerJni.setInstanceForTesting(mBaseSceneLayerJni);
        BookmarkBarSceneLayerJni.setInstanceForTesting(mSceneLayerJni);
        doAnswer(
                        invocation -> {
                            SceneLayer layer = invocation.getArgument(0);
                            layer.setNativePtr(NATIVE_PTR);
                            mNativePtr = JniRawPtr.createForTesting(NATIVE_PTR);
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
        mSceneLayer = new BookmarkBarSceneLayer(mResourceManager);
        mSceneLayer.initializeNative();
    }

    @After
    public void tearDown() {
        mSceneLayer.destroy();
    }

    @Test
    public void testSetVisibility() {
        assertFalse(mSceneLayer.isSceneOverlayTreeShowing());

        mSceneLayer.setVisibility(true);
        assertTrue(mSceneLayer.isSceneOverlayTreeShowing());
        verify(mSceneLayerJni, times(1)).showBookmarkBar(eq(mNativePtr));
        verify(mSceneLayerJni, never()).hideBookmarkBar(any());
        Mockito.reset(mSceneLayerJni);

        mSceneLayer.setVisibility(false);
        assertFalse(mSceneLayer.isSceneOverlayTreeShowing());
        verify(mSceneLayerJni, never()).showBookmarkBar(any());
        verify(mSceneLayerJni, times(1)).hideBookmarkBar(eq(mNativePtr));
        Mockito.reset(mSceneLayerJni);

        mSceneLayer.setVisibility(false);
        verify(mSceneLayerJni, never()).showBookmarkBar(any());
        verify(mSceneLayerJni, never()).hideBookmarkBar(any());
    }

    @Test
    public void testSetContentTree() {
        SceneLayer contentTree = Mockito.mock(SceneLayer.class);
        mSceneLayer.setContentTree(contentTree);

        verify(mSceneLayerJni).setContentTree(eq(mNativePtr), eq(contentTree));
    }
}
