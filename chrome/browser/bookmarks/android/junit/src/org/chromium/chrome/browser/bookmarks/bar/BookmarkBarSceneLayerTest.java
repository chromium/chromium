// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.ui.resources.ResourceManager;

/** Unit tests for {BookmarkBarSceneLayer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BookmarkBarSceneLayerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private BookmarkBarSceneLayerJni mSceneLayerJni;
    @Mock private ResourceManager mResourceManager;

    private static final long NATIVE_PTR = 123L;
    private BookmarkBarSceneLayer mSceneLayer;

    @Before
    public void setUp() {
        BookmarkBarSceneLayerJni.setInstanceForTesting(mSceneLayerJni);
        doReturn(NATIVE_PTR).when(mSceneLayerJni).init(any());
        mSceneLayer = new BookmarkBarSceneLayer(mResourceManager);
        mSceneLayer.initializeNative();
    }

    @Test
    public void testSetVisibility() {
        assertFalse(mSceneLayer.isSceneOverlayTreeShowing());

        mSceneLayer.setVisibility(true);
        assertTrue(mSceneLayer.isSceneOverlayTreeShowing());
        verify(mSceneLayerJni, times(1)).showBookmarkBar(eq(NATIVE_PTR));
        verify(mSceneLayerJni, never()).hideBookmarkBar(anyInt());
        Mockito.reset(mSceneLayerJni);

        mSceneLayer.setVisibility(false);
        assertFalse(mSceneLayer.isSceneOverlayTreeShowing());
        verify(mSceneLayerJni, never()).showBookmarkBar(anyInt());
        verify(mSceneLayerJni, times(1)).hideBookmarkBar(eq(NATIVE_PTR));
        Mockito.reset(mSceneLayerJni);

        mSceneLayer.setVisibility(false);
        verify(mSceneLayerJni, never()).showBookmarkBar(anyInt());
        verify(mSceneLayerJni, never()).hideBookmarkBar(anyInt());
    }

    @Test
    public void testSetContentTree() {
        SceneLayer contentTree = Mockito.mock(SceneLayer.class);
        mSceneLayer.setContentTree(contentTree);

        verify(mSceneLayerJni).setContentTree(eq(NATIVE_PTR), eq(contentTree));
    }
}
