// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.net.Uri;

import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.PostMessageBackend;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

/** Unit tests for {@link PostMessageHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PostMessageHandlerTest {
    private static final Uri SOURCE_URI = Uri.parse("android-app://org.chromium.test");
    private static final Uri TARGET_URI = Uri.parse("https://www.example.com");

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PostMessageBackend mPostMessageBackend;

    @Mock(extraInterfaces = WebContentsObserver.Observable.class)
    private WebContents mWebContents;

    @Mock private NavigationHandle mNavigation;
    @Mock private MessagePort mFirstLocalPort;
    @Mock private MessagePort mFirstRemotePort;
    @Mock private MessagePort mSecondLocalPort;
    @Mock private MessagePort mSecondRemotePort;
    @Captor private ArgumentCaptor<WebContentsObserver> mObserverCaptor;

    private PostMessageHandler mHandler;

    @Before
    public void setUp() {
        when(mWebContents.createMessageChannel())
                .thenReturn(new MessagePort[] {mFirstLocalPort, mFirstRemotePort})
                .thenReturn(new MessagePort[] {mSecondLocalPort, mSecondRemotePort});
        when(mNavigation.hasCommitted()).thenReturn(true);
        when(mNavigation.isSameDocument()).thenReturn(false);
        mHandler = new PostMessageHandler(mPostMessageBackend);
    }

    @Test
    public void testReinitializeChannelAfterMainFrameNavigation() {
        WebContentsObserver observer = resetAndFinishInitialNavigation();
        mHandler.initializeWithPostMessageUri(SOURCE_URI, TARGET_URI);
        verify(mPostMessageBackend).onNotifyMessageChannelReady(null);

        observer.didFinishNavigationInPrimaryMainFrame(mNavigation);
        verify(mFirstLocalPort).close();
        verify(mPostMessageBackend).onDisconnectChannel(any());
        verify((WebContentsObserver.Observable) mWebContents, never()).removeObserver(observer);
        assertNull(mHandler.getPostMessageUriForTesting());
        assertEquals(
                CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR,
                mHandler.postMessageFromClientApp("before re-verification"));

        mHandler.initializeWithPostMessageUri(SOURCE_URI, TARGET_URI);

        assertEquals(
                CustomTabsService.RESULT_SUCCESS,
                mHandler.postMessageFromClientApp("after re-verification"));
        verify(mWebContents, times(2)).createMessageChannel();
        verify(mPostMessageBackend, times(2)).onNotifyMessageChannelReady(null);
    }

    @Test
    public void testSameDocumentNavigationKeepsChannel() {
        WebContentsObserver observer = resetAndFinishInitialNavigation();
        mHandler.initializeWithPostMessageUri(SOURCE_URI, TARGET_URI);
        when(mNavigation.isSameDocument()).thenReturn(true);

        observer.didFinishNavigationInPrimaryMainFrame(mNavigation);

        verify(mFirstLocalPort, never()).close();
        verify(mPostMessageBackend, never()).onDisconnectChannel(any());
        assertEquals(SOURCE_URI, mHandler.getPostMessageUriForTesting());
        assertEquals(
                CustomTabsService.RESULT_SUCCESS,
                mHandler.postMessageFromClientApp("after same-document navigation"));
        verify(mWebContents).createMessageChannel();
        verify(mPostMessageBackend).onNotifyMessageChannelReady(null);
    }

    private WebContentsObserver resetAndFinishInitialNavigation() {
        mHandler.reset(mWebContents);
        verify((WebContentsObserver.Observable) mWebContents)
                .addObserver(mObserverCaptor.capture());
        WebContentsObserver observer = mObserverCaptor.getValue();
        observer.didFinishNavigationInPrimaryMainFrame(mNavigation);
        return observer;
    }
}
