// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewAndroidDelegate;

/** Unit tests for {@link OffscreenRenderingManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class OffscreenRenderingManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private OffscreenRenderingManager.Natives mNativeMock;
    @Mock private WebContents mWebContents1;
    @Mock private WebContents mWebContents2;
    @Mock private Tab mTab;
    @Mock private ViewAndroidDelegate mViewAndroidDelegate;

    private OffscreenRenderingManager mManager;

    @Before
    public void setUp() {
        OffscreenRenderingManagerJni.setInstanceForTesting(mNativeMock);
        when(mNativeMock.init(any(), anyInt(), anyInt())).thenReturn(12345L);

        mManager = OffscreenRenderingManager.getInstance();
    }

    @After
    public void tearDown() {
        mManager.destroy();
    }

    @Test
    public void testStartAndStopOffscreenRenderingForWebContents() {
        when(mWebContents1.getViewAndroidDelegate()).thenReturn(mViewAndroidDelegate);

        mManager.startOffscreenRenderingForWebContents(mWebContents1, 800, 600);

        verify(mNativeMock).init(any(), eq(1), eq(1));
        verify(mWebContents1).setSize(800, 600);
        verify(mNativeMock)
                .startOffscreenRendering(eq(12345L), eq(mWebContents1), eq(800), eq(600));
        verify(mWebContents1).setTopLevelNativeWindow(mManager.getOffscreenWindow());
        verify(mWebContents1).updateWebContentsVisibility(Visibility.VISIBLE);

        when(mWebContents1.getTopLevelNativeWindow()).thenReturn(mManager.getOffscreenWindow());
        mManager.stopOffscreenRendering(mWebContents1);

        verify(mNativeMock).stopOffscreenRendering(eq(12345L), eq(mWebContents1));
        verify(mWebContents1).setTopLevelNativeWindow(isNull());
        verify(mNativeMock).destroy(eq(12345L));
    }

    @Test
    public void testStartOffscreenRenderingForWebContents_setsDelegatesWhenNull() {
        when(mWebContents1.getViewAndroidDelegate()).thenReturn(null);

        mManager.startOffscreenRenderingForWebContents(mWebContents1, 400, 300);

        verify(mWebContents1).setDelegates(eq(""), any(), isNull(), any(), any());
        verify(mNativeMock)
                .startOffscreenRendering(eq(12345L), eq(mWebContents1), eq(400), eq(300));
    }

    @Test
    public void testStartOffscreenRenderingForWebContents_destroyedIgnored() {
        when(mWebContents1.isDestroyed()).thenReturn(true);

        mManager.startOffscreenRenderingForWebContents(mWebContents1, 400, 300);

        verify(mNativeMock, never()).startOffscreenRendering(anyLong(), any(), anyInt(), anyInt());
    }

    @Test
    public void testMultipleWebContentsLifecycle() {
        when(mWebContents1.getViewAndroidDelegate()).thenReturn(mViewAndroidDelegate);
        when(mWebContents2.getViewAndroidDelegate()).thenReturn(mViewAndroidDelegate);

        mManager.startOffscreenRenderingForWebContents(mWebContents1, 800, 600);
        mManager.startOffscreenRenderingForWebContents(mWebContents2, 800, 600);

        mManager.stopOffscreenRendering(mWebContents1);
        verify(mNativeMock, never()).destroy(anyLong());

        mManager.stopOffscreenRendering(mWebContents2);
        verify(mNativeMock).destroy(eq(12345L));
    }

    @Test
    public void testStartAndStopOffscreenRenderingForTab() {
        when(mTab.getWebContents()).thenReturn(mWebContents1);

        mManager.startOffscreenRendering(mTab, 800, 600);

        verify(mNativeMock).init(any(), eq(1), eq(1));
        verify(mWebContents1).setSize(800, 600);
        verify(mNativeMock)
                .startOffscreenRendering(eq(12345L), eq(mWebContents1), eq(800), eq(600));
        verify(mTab).startOffscreenRendering();
        verify(mWebContents1).setTopLevelNativeWindow(any());

        mManager.stopOffscreenRendering(mTab);

        verify(mNativeMock).stopOffscreenRendering(eq(12345L), eq(mWebContents1));
        verify(mTab).stopOffscreenRendering();
        verify(mNativeMock).destroy(eq(12345L));
    }
}
