// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** Unit tests for {@link PageContextBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PageContextBridgeTest {
    private static final String URL = "https://www.example.com/form";
    private static final byte[] PAGE_CONTEXT = new byte[] {1, 2, 3};

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PageContextBridge.Natives mNativeMock;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;

    @Before
    public void setUp() {
        PageContextBridgeJni.setInstanceForTesting(mNativeMock);
    }

    @Test
    public void testFillWebContents_NullPageContext_DoesNothing() {
        PageContextBridge.fillWebContents(mTab, null, URL);

        verify(mTab, never()).addObserver(any());
        verifyNoMoreInteractions(mNativeMock);
    }

    @Test
    public void testFillWebContents_EmptyPageContext_DoesNothing() {
        PageContextBridge.fillWebContents(mTab, new byte[0], URL);

        verify(mTab, never()).addObserver(any());
        verifyNoMoreInteractions(mNativeMock);
    }

    @Test
    public void testFillWebContents_DestroyedTab_DoesNothing() {
        when(mTab.isDestroyed()).thenReturn(true);
        when(mTab.getWebContents()).thenReturn(mWebContents);

        PageContextBridge.fillWebContents(mTab, PAGE_CONTEXT, URL);

        verify(mTab, never()).addObserver(any());
        verifyNoMoreInteractions(mNativeMock);
    }

    @Test
    public void testFillWebContents_ImmediateWebContents_InvokesNativeDirectly() {
        when(mTab.getWebContents()).thenReturn(mWebContents);

        PageContextBridge.fillWebContents(mTab, PAGE_CONTEXT, URL);

        verify(mTab, never()).addObserver(any());
        verify(mNativeMock).fillWebContents(eq(mWebContents), eq(PAGE_CONTEXT), eq(new GURL(URL)));
    }

    @Test
    public void testFillWebContents_DeferredWebContents_WaitsForContentChangedAndUnregisters() {
        PageContextBridge.fillWebContents(mTab, PAGE_CONTEXT, URL);

        ArgumentCaptor<TabObserver> observerCaptor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(observerCaptor.capture());

        // Null WebContents on intermediate onContentChanged keeps observer registered.
        observerCaptor.getValue().onContentChanged(mTab);
        verify(mTab, never()).removeObserver(any());
        verifyNoMoreInteractions(mNativeMock);

        when(mTab.getWebContents()).thenReturn(mWebContents);
        observerCaptor.getValue().onContentChanged(mTab);

        verify(mTab).removeObserver(observerCaptor.getValue());
        verify(mNativeMock).fillWebContents(eq(mWebContents), eq(PAGE_CONTEXT), eq(new GURL(URL)));
    }

    @Test
    public void testFillWebContents_DeferredTabDestroyed_UnregistersWithoutCallingNative() {
        PageContextBridge.fillWebContents(mTab, PAGE_CONTEXT, URL);

        ArgumentCaptor<TabObserver> observerCaptor = ArgumentCaptor.forClass(TabObserver.class);
        verify(mTab).addObserver(observerCaptor.capture());

        observerCaptor.getValue().onDestroyed(mTab);

        verify(mTab).removeObserver(observerCaptor.getValue());
        verifyNoMoreInteractions(mNativeMock);
    }
}
