// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.net.Uri;
import android.os.Bundle;

import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.PostMessageBackend;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.TerminationStatus;
import org.chromium.base.TriState;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.content_public.browser.MessagePort.MessageCallback;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.Page;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.mojo_base.mojom.UnguessableToken;
import org.chromium.net.GURLUtils;
import org.chromium.net.GURLUtilsJni;
import org.chromium.url.GURL;

/** Unit tests for {@link PostMessageHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PostMessageHandlerTest {
    private static final Uri SOURCE_URI = Uri.parse("android-app://org.chromium.test");
    private static final Uri TARGET_URI = Uri.parse("https://www.example.com");
    private static final String POST_MESSAGE_ORIGIN =
            "androidx.browser.customtabs.POST_MESSAGE_ORIGIN";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PostMessageBackend mPostMessageBackend;

    @Mock(extraInterfaces = WebContentsObserver.Observable.class)
    private WebContents mWebContents;

    @Mock(extraInterfaces = WebContentsObserver.Observable.class)
    private WebContents mSecondWebContents;

    @Mock private RenderFrameHost mMainFrame;
    @Mock private NavigationHandle mNavigation;
    @Mock private MessagePort mFirstLocalPort;
    @Mock private MessagePort mFirstRemotePort;
    @Mock private MessagePort mSecondLocalPort;
    @Mock private MessagePort mSecondRemotePort;
    @Captor private ArgumentCaptor<WebContentsObserver> mObserverCaptor;
    @Mock private GURLUtils.Natives mGURLUtilsJni;

    private PostMessageHandler mHandler;

    @Before
    public void setUp() {
        lenient().when(mWebContents.getMainFrame()).thenReturn(mMainFrame);
        lenient()
                .when(mMainFrame.getLastCommittedURL())
                .thenReturn(new GURL("https://www.example.com/page"));
        lenient()
                .when(mWebContents.createMessageChannel())
                .thenReturn(new MessagePort[] {mFirstLocalPort, mFirstRemotePort})
                .thenReturn(new MessagePort[] {mSecondLocalPort, mSecondRemotePort});
        lenient()
                .when(mSecondWebContents.createMessageChannel())
                .thenReturn(new MessagePort[] {mFirstLocalPort, mFirstRemotePort});
        lenient().when(mNavigation.hasCommitted()).thenReturn(true);
        lenient().when(mNavigation.isSameDocument()).thenReturn(false);
        GURLUtilsJni.setInstanceForTesting(mGURLUtilsJni);
        lenient().when(mGURLUtilsJni.getOrigin(any())).thenReturn("https://www.example.com");
        mHandler = new PostMessageHandler(mPostMessageBackend);
    }

    @After
    public void tearDown() {
        GURLUtilsJni.setInstanceForTesting(null);
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

    @Test
    public void testTargetOriginDefaultsToVerifiedOriginWhenTargetNull() {
        mHandler.reset(mWebContents);
        mHandler.onOriginVerified(
                "org.chromium.test", Origin.create(TARGET_URI), true, TriState.TRUE);

        assertEquals(TARGET_URI, mHandler.getPostMessageTargetUriForTesting());
        verify(mWebContents).createMessageChannel();
        verify(mWebContents)
                .postMessageToMainFrame(
                        any(MessagePayload.class), any(), eq(TARGET_URI.toString()), any());
        verify(mPostMessageBackend).onNotifyMessageChannelReady(null);
    }

    @Test
    public void testOriginMismatchPreventsInitializationOnCommittedPage() {
        when(mMainFrame.getLastCommittedURL()).thenReturn(new GURL("https://other.example.com"));

        mHandler.reset(mWebContents);
        mHandler.initializeWithPostMessageUri(SOURCE_URI, TARGET_URI);

        verify(mWebContents, never()).createMessageChannel();
        verify(mPostMessageBackend, never()).onNotifyMessageChannelReady(any());
        assertEquals(
                CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR,
                mHandler.postMessageFromClientApp("sample_data"));
    }

    @Test
    public void testOriginMatchAllowsInitializationOnCommittedPage() {
        when(mMainFrame.getLastCommittedURL())
                .thenReturn(new GURL("https://www.example.com/page2"));

        mHandler.reset(mWebContents);
        mHandler.initializeWithPostMessageUri(SOURCE_URI, TARGET_URI);

        verify(mWebContents).createMessageChannel();
        verify(mPostMessageBackend).onNotifyMessageChannelReady(null);
    }

    @Test
    public void testVerificationCompletionAfterCrossDocumentNavigationToDifferentOrigin() {
        WebContentsObserver observer = resetAndFinishInitialNavigation();

        // Simulate cross-document navigation committing to a different origin.
        when(mMainFrame.getLastCommittedURL()).thenReturn(new GURL("https://other.example.com"));
        observer.didFinishNavigationInPrimaryMainFrame(mNavigation);

        // Verification completes for the initial requested origin after navigation.
        mHandler.onOriginVerified(
                "org.chromium.test", Origin.create(TARGET_URI), true, TriState.TRUE);

        // Channel creation should be rejected due to origin mismatch on the active document.
        verify(mWebContents, never()).createMessageChannel();
        verify(mPostMessageBackend, never()).onNotifyMessageChannelReady(any());
    }

    @Test
    public void testResetWithNullWebContentsCleansUpStateWhenChannelNull() {
        mHandler.reset(mWebContents);
        verify((WebContentsObserver.Observable) mWebContents)
                .addObserver(mObserverCaptor.capture());
        WebContentsObserver observer = mObserverCaptor.getValue();

        assertNotNull(mHandler.getWebContentsForTesting());
        assertNotNull(mHandler.getWebContentsObserverForTesting());

        mHandler.reset(null);

        assertNull(mHandler.getWebContentsForTesting());
        assertNull(mHandler.getWebContentsObserverForTesting());
        verify((WebContentsObserver.Observable) mWebContents).removeObserver(observer);

        // Subsequent verification completion should not initialize on forgotten WebContents.
        mHandler.onOriginVerified(
                "org.chromium.test", Origin.create(TARGET_URI), true, TriState.TRUE);

        verify(mWebContents, never()).createMessageChannel();
        verify(mPostMessageBackend, never()).onNotifyMessageChannelReady(any());
    }

    @Test
    public void testResetWithDestroyedWebContentsCleansUpState() {
        WebContentsObserver observer = resetAndFinishInitialNavigation();
        assertNotNull(mHandler.getWebContentsForTesting());
        when(mWebContents.isDestroyed()).thenReturn(true);

        mHandler.reset(mWebContents);

        assertNull(mHandler.getWebContentsForTesting());
        assertNull(mHandler.getWebContentsObserverForTesting());
        verify((WebContentsObserver.Observable) mWebContents).removeObserver(observer);
    }

    @Test
    public void testPrimaryMainFrameRenderProcessGoneCleansUpWhenChannelNull() {
        mHandler.reset(mWebContents);
        verify((WebContentsObserver.Observable) mWebContents)
                .addObserver(mObserverCaptor.capture());
        WebContentsObserver observer = mObserverCaptor.getValue();

        // Render process gone while channel is not yet initialized.
        observer.primaryMainFrameRenderProcessGone(TerminationStatus.PROCESS_WAS_KILLED);

        assertNull(mHandler.getWebContentsForTesting());
        assertNull(mHandler.getWebContentsObserverForTesting());
        verify((WebContentsObserver.Observable) mWebContents).removeObserver(observer);

        mHandler.onOriginVerified(
                "org.chromium.test", Origin.create(TARGET_URI), true, TriState.TRUE);

        verify(mWebContents, never()).createMessageChannel();
        verify(mPostMessageBackend, never()).onNotifyMessageChannelReady(any());
    }

    @Test
    public void testWebContentsDestroyedCleansUpWhenChannelNull() {
        mHandler.reset(mWebContents);
        verify((WebContentsObserver.Observable) mWebContents)
                .addObserver(mObserverCaptor.capture());
        WebContentsObserver observer = mObserverCaptor.getValue();

        observer.webContentsDestroyed();

        assertNull(mHandler.getWebContentsForTesting());
        assertNull(mHandler.getWebContentsObserverForTesting());
        verify((WebContentsObserver.Observable) mWebContents).removeObserver(observer);

        mHandler.onOriginVerified(
                "org.chromium.test", Origin.create(TARGET_URI), true, TriState.TRUE);

        verify(mWebContents, never()).createMessageChannel();
        verify(mPostMessageBackend, never()).onNotifyMessageChannelReady(any());
    }

    @Test
    public void testResetWithDifferentWebContentsCleansUpPreviousObserver() {
        mHandler.reset(mWebContents);
        verify((WebContentsObserver.Observable) mWebContents)
                .addObserver(mObserverCaptor.capture());
        WebContentsObserver firstObserver = mObserverCaptor.getValue();

        mHandler.reset(mSecondWebContents);

        verify((WebContentsObserver.Observable) mWebContents).removeObserver(firstObserver);
        verify((WebContentsObserver.Observable) mSecondWebContents).addObserver(any());
        assertEquals(mSecondWebContents, mHandler.getWebContentsForTesting());
    }

    @Test
    public void
            testInitializeWithPostMessageUriBeforeResetKeepsUriAndInitializesOnDocumentLoaded() {
        mHandler.initializeWithPostMessageUri(SOURCE_URI, TARGET_URI);
        mHandler.reset(mWebContents);

        assertEquals(SOURCE_URI, mHandler.getPostMessageUriForTesting());

        verify((WebContentsObserver.Observable) mWebContents)
                .addObserver(mObserverCaptor.capture());
        WebContentsObserver observer = mObserverCaptor.getValue();

        observer.documentLoadedInPrimaryMainFrame(
                mock(Page.class), new GlobalRenderFrameHostId(1, 1), LifecycleState.ACTIVE);

        verify(mWebContents).createMessageChannel();
        verify(mPostMessageBackend).onNotifyMessageChannelReady(null);
    }

    @Test
    public void testUncommittedNavigationDoesNotAdvanceNavigationState() {
        mHandler.reset(mWebContents);
        verify((WebContentsObserver.Observable) mWebContents)
                .addObserver(mObserverCaptor.capture());
        WebContentsObserver observer = mObserverCaptor.getValue();

        when(mNavigation.hasCommitted()).thenReturn(false);
        observer.didFinishNavigationInPrimaryMainFrame(mNavigation);

        mHandler.initializeWithPostMessageUri(SOURCE_URI, TARGET_URI);
        verify(mPostMessageBackend).onNotifyMessageChannelReady(null);

        when(mNavigation.hasCommitted()).thenReturn(true);
        observer.didFinishNavigationInPrimaryMainFrame(mNavigation);

        verify(mFirstLocalPort, never()).close();
        verify(mPostMessageBackend, never()).onDisconnectChannel(any());
        assertEquals(SOURCE_URI, mHandler.getPostMessageUriForTesting());
    }

    @Test
    public void testCrossDocumentNavigationWithoutChannelResetsTargetUri() {
        WebContentsObserver observer = resetAndFinishInitialNavigation();
        mHandler.setPostMessageTargetUri(TARGET_URI);
        assertEquals(TARGET_URI, mHandler.getPostMessageTargetUriForTesting());

        observer.didFinishNavigationInPrimaryMainFrame(mNavigation);

        assertNull(mHandler.getPostMessageUriForTesting());
        assertNull(mHandler.getPostMessageTargetUriForTesting());
    }

    @Test
    public void testInitializeWithWebContentsDefersWhenUrlUncommitted() {
        when(mMainFrame.getLastCommittedURL()).thenReturn(GURL.emptyGURL());

        mHandler.reset(mWebContents);
        verify((WebContentsObserver.Observable) mWebContents)
                .addObserver(mObserverCaptor.capture());
        WebContentsObserver observer = mObserverCaptor.getValue();

        mHandler.onOriginVerified(
                "org.chromium.test", Origin.create(TARGET_URI), true, TriState.TRUE);

        verify(mWebContents, never()).createMessageChannel();
        verify(mPostMessageBackend, never()).onNotifyMessageChannelReady(any());

        when(mMainFrame.getLastCommittedURL()).thenReturn(new GURL("https://www.example.com/page"));
        observer.documentLoadedInPrimaryMainFrame(
                mock(Page.class), new GlobalRenderFrameHostId(1, 1), LifecycleState.ACTIVE);

        verify(mWebContents).createMessageChannel();
        verify(mPostMessageBackend).onNotifyMessageChannelReady(null);
    }

    @Test
    public void testInitializeWithWebContentsClosesExistingChannelOnOriginMismatch() {
        resetAndFinishInitialNavigation();
        mHandler.initializeWithPostMessageUri(SOURCE_URI, TARGET_URI);
        verify(mPostMessageBackend).onNotifyMessageChannelReady(null);

        when(mMainFrame.getLastCommittedURL())
                .thenReturn(new GURL("https://other.example.com/page"));
        mHandler.initializeWithPostMessageUri(SOURCE_URI, TARGET_URI);

        verify(mFirstLocalPort).close();
        verify(mPostMessageBackend).onDisconnectChannel(any());
        verify(mWebContents, times(1)).createMessageChannel();
    }

    @Test
    public void testInitializeWithWebContentsReplacesExistingChannelWhenOriginMatches() {
        resetAndFinishInitialNavigation();
        mHandler.initializeWithPostMessageUri(SOURCE_URI, TARGET_URI);
        verify(mPostMessageBackend).onNotifyMessageChannelReady(null);

        mHandler.initializeWithPostMessageUri(SOURCE_URI, TARGET_URI);

        verify(mFirstLocalPort).close();
        verify(mPostMessageBackend, never()).onDisconnectChannel(any());
        verify(mWebContents, times(2)).createMessageChannel();
        verify(mPostMessageBackend, times(2)).onNotifyMessageChannelReady(null);
    }

    @Test
    public void testMessageCallbackSendsOriginInBundle() {
        MessageCallback callback = initializeAndCaptureMessageCallback();

        callback.onMessage(new MessagePayload("test_message"), null);

        ArgumentCaptor<Bundle> bundleCaptor = ArgumentCaptor.forClass(Bundle.class);
        verify(mPostMessageBackend).onPostMessage(eq("test_message"), bundleCaptor.capture());
        Bundle bundle = bundleCaptor.getValue();
        assertNotNull(bundle);
        assertEquals("https://www.example.com", bundle.getString(POST_MESSAGE_ORIGIN));
    }

    @Test
    public void testMessageCallbackUsesOriginCapturedAtChannelCreation() {
        MessageCallback callback = initializeAndCaptureMessageCallback();

        // The document navigates away after the channel was created. A message already queued on
        // the UI task runner must still be attributed to the document that sent it, not to
        // whatever happens to be committed by the time it is delivered. These stubs are lenient
        // precisely because the handler must no longer consult them.
        lenient()
                .when(mMainFrame.getLastCommittedURL())
                .thenReturn(new GURL("https://other.example.com/p"));
        lenient().when(mGURLUtilsJni.getOrigin(any())).thenReturn("https://other.example.com");

        callback.onMessage(new MessagePayload("test_message"), null);

        ArgumentCaptor<Bundle> bundleCaptor = ArgumentCaptor.forClass(Bundle.class);
        verify(mPostMessageBackend).onPostMessage(eq("test_message"), bundleCaptor.capture());
        assertEquals(
                "https://www.example.com", bundleCaptor.getValue().getString(POST_MESSAGE_ORIGIN));
    }

    @Test
    public void testMessageCallbackWithEmptyOriginSendsNullBundle() {
        when(mGURLUtilsJni.getOrigin(any())).thenReturn("");
        MessageCallback callback = initializeAndCaptureMessageCallback();

        callback.onMessage(new MessagePayload("test_message"), null);

        verify(mPostMessageBackend).onPostMessage("test_message", null);
    }

    @Test
    public void testMessageCallbackWithNullMainFrameUrlSendsNullBundle() {
        when(mMainFrame.getLastCommittedURL()).thenReturn(null);
        MessageCallback callback = initializeAndCaptureMessageCallback(/* targetUri= */ null);

        callback.onMessage(new MessagePayload("test_message"), null);

        verify(mPostMessageBackend).onPostMessage("test_message", null);
    }

    @Test
    public void testMessageCallbackWithEmptyMainFrameUrlSendsNullBundle() {
        when(mMainFrame.getLastCommittedURL()).thenReturn(GURL.emptyGURL());
        MessageCallback callback = initializeAndCaptureMessageCallback(/* targetUri= */ null);

        callback.onMessage(new MessagePayload("test_message"), null);

        verify(mPostMessageBackend).onPostMessage("test_message", null);
    }

    @Test
    public void testMessageCallbackWithInvalidMainFrameUrlSendsNullBundle() {
        when(mMainFrame.getLastCommittedURL()).thenReturn(new GURL("invalid url"));
        MessageCallback callback = initializeAndCaptureMessageCallback(/* targetUri= */ null);

        callback.onMessage(new MessagePayload("test_message"), null);

        verify(mPostMessageBackend).onPostMessage("test_message", null);
    }

    @Test
    public void testMessageCallbackWithNullMainFrameSendsNullBundle() {
        when(mWebContents.getMainFrame()).thenReturn(null);
        MessageCallback callback = initializeAndCaptureMessageCallback(/* targetUri= */ null);

        callback.onMessage(new MessagePayload("test_message"), null);

        verify(mPostMessageBackend).onPostMessage("test_message", null);
    }

    @Test
    public void testMessageCallbackDiscardsWhenChannelTransferred() {
        MessageCallback callback = initializeAndCaptureMessageCallback();
        when(mFirstLocalPort.isTransferred()).thenReturn(true);

        callback.onMessage(new MessagePayload("test_message"), null);

        verify(mPostMessageBackend, never()).onPostMessage(any(), any());
    }

    @Test
    public void testMessageCallbackDiscardsWhenWebContentsDestroyed() {
        MessageCallback callback = initializeAndCaptureMessageCallback();
        when(mWebContents.isDestroyed()).thenReturn(true);

        callback.onMessage(new MessagePayload("test_message"), null);

        verify(mPostMessageBackend, never()).onPostMessage(any(), any());
    }

    @Test
    public void testMessageCallbackDiscardsWhenChannelNull() {
        MessageCallback callback = initializeAndCaptureMessageCallback();
        mHandler.reset(null);

        callback.onMessage(new MessagePayload("test_message"), null);

        verify(mPostMessageBackend, never()).onPostMessage(any(), any());
    }

    @Test
    public void testPostMessageFromClientAppFailsWhenChannelTransferred() {
        mHandler.reset(mWebContents);
        mHandler.initializeWithPostMessageUri(SOURCE_URI, TARGET_URI);
        when(mFirstLocalPort.isTransferred()).thenReturn(true);

        assertEquals(
                CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR,
                mHandler.postMessageFromClientApp("test_message"));
    }

    @Test
    public void testPostMessageFromClientAppFailsWhenChannelClosed() {
        mHandler.reset(mWebContents);
        mHandler.initializeWithPostMessageUri(SOURCE_URI, TARGET_URI);
        when(mFirstLocalPort.isClosed()).thenReturn(true);

        assertEquals(
                CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR,
                mHandler.postMessageFromClientApp("test_message"));
    }

    @Test
    public void testPostMessageFromClientAppFailsWhenWebContentsDestroyed() {
        mHandler.reset(mWebContents);
        mHandler.initializeWithPostMessageUri(SOURCE_URI, TARGET_URI);
        when(mWebContents.isDestroyed()).thenReturn(true);

        assertEquals(
                CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR,
                mHandler.postMessageFromClientApp("test_message"));
    }

    @Test
    public void testPostMessageFromClientAppFailsWhenChannelNull() {
        mHandler.reset(mWebContents);

        assertEquals(
                CustomTabsService.RESULT_FAILURE_MESSAGING_ERROR,
                mHandler.postMessageFromClientApp("test_message"));
    }

    @Test
    public void testWildcardTargetOriginAllowsInitialization() {
        resetAndFinishInitialNavigation();
        when(mMainFrame.getLastCommittedURL())
                .thenReturn(new GURL("https://other.example.com/page"));

        mHandler.initializeWithPostMessageUri(SOURCE_URI, Uri.parse("*"));

        // "*" is a wildcard target origin, so a mismatched committed origin must not block the
        // channel.
        verify(mWebContents).createMessageChannel();
        verify(mPostMessageBackend).onNotifyMessageChannelReady(null);
    }

    @Test
    public void testOpaqueOriginPreventsChannelInitialization() {
        resetAndFinishInitialNavigation();
        // A sandboxed frame keeps an https:// URL but has an opaque security context, so the URL
        // comparison alone would wrongly let the channel through.
        when(mMainFrame.getLastCommittedOrigin()).thenReturn(createOpaqueOrigin());

        mHandler.initializeWithPostMessageUri(SOURCE_URI, TARGET_URI);

        verify(mWebContents, never()).createMessageChannel();
        verify(mPostMessageBackend, never()).onNotifyMessageChannelReady(any());
    }

    @Test
    public void testResetWithCommittedWebContentsTearsDownOnNextNavigation() {
        // Simulates promoting a hidden tab or swapping in a tab that has already loaded a
        // document: the next cross-document navigation is a real document change, not the initial
        // one, so the channel must not survive it.
        when(mWebContents.getLastCommittedUrl()).thenReturn(new GURL("https://www.example.com/p"));

        mHandler.reset(mWebContents);
        verify((WebContentsObserver.Observable) mWebContents)
                .addObserver(mObserverCaptor.capture());
        WebContentsObserver observer = mObserverCaptor.getValue();
        mHandler.initializeWithPostMessageUri(SOURCE_URI, TARGET_URI);
        verify(mPostMessageBackend).onNotifyMessageChannelReady(null);

        observer.didFinishNavigationInPrimaryMainFrame(mNavigation);

        verify(mFirstLocalPort).close();
        verify(mPostMessageBackend).onDisconnectChannel(any());
        assertNull(mHandler.getPostMessageUriForTesting());
        assertNull(mHandler.getPostMessageTargetUriForTesting());
    }

    @Test
    public void testCloseChannelDoesNotCloseTransferredPort() {
        WebContentsObserver observer = resetAndFinishInitialNavigation();
        mHandler.initializeWithPostMessageUri(SOURCE_URI, TARGET_URI);
        // MessagePort#close() throws once the port has been transferred.
        when(mFirstLocalPort.isTransferred()).thenReturn(true);

        observer.didFinishNavigationInPrimaryMainFrame(mNavigation);

        verify(mFirstLocalPort, never()).close();
        verify(mPostMessageBackend).onDisconnectChannel(any());
    }

    @Test
    public void testPostMessageFromClientAppDropsWhenPortTransferredBeforeTaskRuns() {
        mHandler.reset(mWebContents);
        mHandler.initializeWithPostMessageUri(SOURCE_URI, TARGET_URI);

        // Accepted while the port is still live, but transferred before the posted task runs.
        assertEquals(
                CustomTabsService.RESULT_SUCCESS,
                mHandler.postMessageFromClientApp("test_message"));
        when(mFirstLocalPort.isTransferred()).thenReturn(true);
        ShadowLooper.runUiThreadTasks();

        verify(mFirstLocalPort, never()).postMessage(any(), any());
    }

    /** Builds an opaque {@link org.chromium.url.Origin}, as a sandboxed frame would have. */
    private static org.chromium.url.Origin createOpaqueOrigin() {
        org.chromium.url.internal.mojom.Origin mojom = new org.chromium.url.internal.mojom.Origin();
        mojom.scheme = "https";
        mojom.host = "www.example.com";
        mojom.port = (short) 443;
        UnguessableToken token = new UnguessableToken();
        token.high = 1;
        token.low = 2;
        mojom.nonceIfOpaque = token;
        return new org.chromium.url.Origin(mojom);
    }

    private MessageCallback initializeAndCaptureMessageCallback() {
        return initializeAndCaptureMessageCallback(TARGET_URI);
    }

    private MessageCallback initializeAndCaptureMessageCallback(Uri targetUri) {
        mHandler.reset(mWebContents);
        mHandler.initializeWithPostMessageUri(SOURCE_URI, targetUri);
        ArgumentCaptor<MessageCallback> callbackCaptor =
                ArgumentCaptor.forClass(MessageCallback.class);
        verify(mFirstLocalPort).setMessageCallback(callbackCaptor.capture(), any());
        return callbackCaptor.getValue();
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
