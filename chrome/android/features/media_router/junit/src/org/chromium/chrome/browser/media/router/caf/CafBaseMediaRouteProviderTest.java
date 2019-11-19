// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.media.router.caf.CafBaseMediaRouteProvider.NO_SINKS;

import android.content.Context;
import android.support.v7.media.MediaRouteSelector;
import android.support.v7.media.MediaRouter;

import androidx.annotation.NonNull;

import com.google.android.gms.cast.framework.CastContext;
import com.google.android.gms.cast.framework.CastSession;
import com.google.android.gms.cast.framework.SessionManager;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.media.router.MediaRoute;
import org.chromium.chrome.browser.media.router.MediaRouteManager;
import org.chromium.chrome.browser.media.router.MediaSink;
import org.chromium.chrome.browser.media.router.MediaSource;

import java.util.ArrayList;
import java.util.List;

/**
 * Robolectric tests for CafBaseMediaRouteProvider.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowMediaRouter.class, ShadowCastContext.class, ShadowLooper.class})
public class CafBaseMediaRouteProviderTest {
    private Context mContext;
    private TestMRP mProvider;
    private MediaRouterTestHelper mMediaRouterHelper;
    private MediaRouter mMediaRouter;
    @Mock
    private MediaRouteManager mManager;
    @Mock
    private CastContext mCastContext;
    @Mock
    private CastSession mCastSession;
    @Mock
    private SessionManager mSessionManager;
    @Mock
    private BaseSessionController mSessionController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        mMediaRouterHelper = new MediaRouterTestHelper();
        ShadowCastContext.setInstance(mCastContext);
        mMediaRouter = MediaRouter.getInstance(mContext);
        mProvider = spy(new TestMRP(mMediaRouter, mManager));
        doReturn(mSessionController).when(mProvider).sessionController();
        doReturn(mSessionManager).when(mCastContext).getSessionManager();
    }

    @Test
    public void testOnSinksReceived() {
        List<MediaSink> sinks = new ArrayList<>();
        sinks.add(mock(MediaSink.class));
        sinks.add(mock(MediaSink.class));

        mProvider.onSinksReceived("source-id", sinks);
        ShadowLooper.idleMainLooper();

        verify(mManager).onSinksReceived("source-id", mProvider, sinks);
    }

    @Test
    public void testSupportsSource() {
        // Source unsupported.
        doReturn(null).when(mProvider).getSourceFromId(any(String.class));
        assertFalse(mProvider.supportsSource("source-id"));

        // Source supported.
        doReturn(mock(MediaSource.class)).when(mProvider).getSourceFromId(any(String.class));
        assertTrue(mProvider.supportsSource("source-id"));
    }

    @Test
    public void testStartObservingMediaSinks_unsupportedSource() {
        doReturn(null).when(mProvider).getSourceFromId(any(String.class));

        mProvider.startObservingMediaSinks("source-id");
        ShadowLooper.idleMainLooper();

        verify(mManager).onSinksReceived("source-id", mProvider, NO_SINKS);
        verify(mMediaRouterHelper.getShadowImpl(), never())
                .addCallback(
                        any(MediaRouteSelector.class), any(MediaRouter.Callback.class), anyInt());
        assertTrue(mProvider.mDiscoveryCallbacks.isEmpty());
    }

    @Test
    public void testStartObservingMediaSinks_invalidSelector() {
        MediaSource mockSource = mock(MediaSource.class);
        doReturn(mockSource).when(mProvider).getSourceFromId(any(String.class));
        doReturn(null).when(mockSource).buildRouteSelector();

        mProvider.startObservingMediaSinks("source-id");
        ShadowLooper.idleMainLooper();

        verify(mManager).onSinksReceived("source-id", mProvider, NO_SINKS);
        verify(mMediaRouterHelper.getShadowImpl(), never())
                .addCallback(
                        any(MediaRouteSelector.class), any(MediaRouter.Callback.class), anyInt());
        assertTrue(mProvider.mDiscoveryCallbacks.isEmpty());
    }

    @Test
    @SuppressWarnings("unchecked")
    public void testStartObservingMediaSinks_twoDifferentAppIds() {
        MediaSource mockSource1 = mock(MediaSource.class);
        MediaSource mockSource2 = mock(MediaSource.class);
        MediaRouteSelector mockSelector1 = mock(MediaRouteSelector.class);
        MediaRouteSelector mockSelector2 = mock(MediaRouteSelector.class);
        prepareMediaSource(mockSource1, mockSelector1, "source-id-1", "app-id-1");
        prepareMediaSource(mockSource2, mockSelector2, "source-id-2", "app-id-2");

        mProvider.startObservingMediaSinks("source-id-1");
        mProvider.startObservingMediaSinks("source-id-2");
        ShadowLooper.idleMainLooper();

        // Empty devices are published while the callbacks are constructed.
        verify(mManager).onSinksReceived(eq("source-id-1"), eq(mProvider), eq(NO_SINKS));
        verify(mManager).onSinksReceived(eq("source-id-2"), eq(mProvider), eq(NO_SINKS));
        verify(mMediaRouterHelper.getShadowImpl())
                .addCallback(eq(mockSelector1), any(MediaRouter.Callback.class),
                        eq(MediaRouter.CALLBACK_FLAG_REQUEST_DISCOVERY));
        verify(mMediaRouterHelper.getShadowImpl())
                .addCallback(eq(mockSelector2), any(MediaRouter.Callback.class),
                        eq(MediaRouter.CALLBACK_FLAG_REQUEST_DISCOVERY));

        assertEquals(mProvider.mDiscoveryCallbacks.size(), 2);
        assertTrue(mProvider.mDiscoveryCallbacks.containsKey("app-id-1"));
        assertTrue(mProvider.mDiscoveryCallbacks.containsKey("app-id-2"));

        // Add one route for source 1.
        MediaRouter.RouteInfo routeInfo = mock(MediaRouter.RouteInfo.class);
        doReturn(true).when(routeInfo).matchesSelector(any(MediaRouteSelector.class));
        mProvider.mDiscoveryCallbacks.get("app-id-1").onRouteAdded(mMediaRouter, routeInfo);

        ArgumentCaptor<List<MediaSink>> sinksCaptor = ArgumentCaptor.forClass(List.class);

        verify(mManager, times(2))
                .onSinksReceived(eq("source-id-1"), eq(mProvider), sinksCaptor.capture());
        assertEquals(sinksCaptor.getAllValues().get(1).size(), 1);
    }

    @Test
    @SuppressWarnings("unchecked")
    public void testStartObservingMediaSinks_twoSameAppIds() {
        MediaSource mockSource1 = mock(MediaSource.class);
        MediaSource mockSource2 = mock(MediaSource.class);
        MediaRouteSelector mockSelector1 = mock(MediaRouteSelector.class);
        MediaRouteSelector mockSelector2 = mock(MediaRouteSelector.class);
        prepareMediaSource(mockSource1, mockSelector1, "source-id-1", "app-id-1");
        prepareMediaSource(mockSource2, mockSelector2, "source-id-2", "app-id-1");

        mProvider.startObservingMediaSinks("source-id-1");
        mProvider.startObservingMediaSinks("source-id-2");
        ShadowLooper.idleMainLooper();

        // Empty devices are published while the callbacks are constructed.
        verify(mManager).onSinksReceived(eq("source-id-1"), eq(mProvider), eq(NO_SINKS));
        verify(mManager).onSinksReceived(eq("source-id-2"), eq(mProvider), eq(NO_SINKS));
        verify(mMediaRouterHelper.getShadowImpl())
                .addCallback(eq(mockSelector1), any(MediaRouter.Callback.class),
                        eq(MediaRouter.CALLBACK_FLAG_REQUEST_DISCOVERY));

        assertEquals(mProvider.mDiscoveryCallbacks.size(), 1);
        assertTrue(mProvider.mDiscoveryCallbacks.containsKey("app-id-1"));

        // Add one route for source 1.
        MediaRouter.RouteInfo routeInfo = mock(MediaRouter.RouteInfo.class);
        doReturn(true).when(routeInfo).matchesSelector(any(MediaRouteSelector.class));
        mProvider.mDiscoveryCallbacks.get("app-id-1").onRouteAdded(mMediaRouter, routeInfo);

        ArgumentCaptor<List<MediaSink>> sinksCaptor = ArgumentCaptor.forClass(List.class);

        verify(mManager, times(2))
                .onSinksReceived(eq("source-id-1"), eq(mProvider), sinksCaptor.capture());
        assertEquals(sinksCaptor.getAllValues().get(1).size(), 1);

        sinksCaptor = ArgumentCaptor.forClass(List.class);

        verify(mManager, times(2))
                .onSinksReceived(eq("source-id-2"), eq(mProvider), sinksCaptor.capture());
        assertEquals(sinksCaptor.getAllValues().get(1).size(), 1);
    }

    @Test
    @SuppressWarnings("unchecked")
    public void testStartObservingMediaSinks_hasMatchingRouteOnStart() {
        doReturn(true)
                .when(mMediaRouterHelper.getCastRoute())
                .matchesSelector(any(MediaRouteSelector.class));
        MediaSource mockSource = mock(MediaSource.class);
        MediaRouteSelector mockSelector = mock(MediaRouteSelector.class);
        prepareMediaSource(mockSource, mockSelector, "source-id", "app-id");

        mProvider.startObservingMediaSinks("source-id");
        ShadowLooper.idleMainLooper();

        // Existing devices that match the selector should be published upon start observing.
        ArgumentCaptor<List<MediaSink>> sinksCaptor = ArgumentCaptor.forClass(List.class);
        verify(mManager).onSinksReceived(eq("source-id"), eq(mProvider), sinksCaptor.capture());
        assertEquals(sinksCaptor.getValue().size(), 1);
    }

    @Test
    public void testStopObservingMediaSinks() {
        MediaSource mockSource1 = mock(MediaSource.class);
        MediaSource mockSource2 = mock(MediaSource.class);
        MediaRouteSelector mockSelector1 = mock(MediaRouteSelector.class);
        MediaRouteSelector mockSelector2 = mock(MediaRouteSelector.class);
        prepareMediaSource(mockSource1, mockSelector1, "source-id-1", "app-id-1");
        prepareMediaSource(mockSource2, mockSelector2, "source-id-2", "app-id-1");

        mProvider.startObservingMediaSinks("source-id-1");
        mProvider.startObservingMediaSinks("source-id-2");
        mProvider.stopObservingMediaSinks("source-id-1");

        verify(mMediaRouterHelper.getShadowImpl(), never())
                .removeCallback(any(MediaRouter.Callback.class));
        assertEquals(mProvider.mDiscoveryCallbacks.size(), 1);

        mProvider.stopObservingMediaSinks("source-id-2");

        verify(mMediaRouterHelper.getShadowImpl()).removeCallback(any(MediaRouter.Callback.class));
        assertTrue(mProvider.mDiscoveryCallbacks.isEmpty());
    }

    @Test
    public void testCreateRoute() {
        InOrder inOrder = inOrder(mSessionManager, mSessionController, mManager);
        MediaSource mockSource = mock(MediaSource.class);
        doReturn(mockSource).when(mProvider).getSourceFromId("source-id");

        // Normal case.
        mProvider.createRoute("source-id", "cast-route", "presentation-id", "origin", 1, false, 1);

        inOrder.verify(mSessionManager).addSessionManagerListener(mProvider, CastSession.class);
        inOrder.verify(mSessionController).requestSessionLaunch();
        CreateRouteRequestInfo pendingCreateRouteRequestInfo =
                mProvider.getPendingCreateRouteRequestInfo();
        assertEquals(pendingCreateRouteRequestInfo.source, mockSource);
        assertEquals(pendingCreateRouteRequestInfo.sink.getId(), "cast-route");
        assertEquals(pendingCreateRouteRequestInfo.presentationId, "presentation-id");
        assertEquals(pendingCreateRouteRequestInfo.origin, "origin");
        assertEquals(pendingCreateRouteRequestInfo.tabId, 1);
        assertEquals(pendingCreateRouteRequestInfo.isIncognito, false);
        assertEquals(pendingCreateRouteRequestInfo.nativeRequestId, 1);
        assertEquals(pendingCreateRouteRequestInfo.routeInfo, mMediaRouterHelper.getCastRoute());

        // Second request cancels the first request.
        mProvider.createRoute(
                "source-id", "other-cast-route", "presentation-id-2", "origin-2", 2, true, 2);

        inOrder.verify(mManager).onRouteRequestError(eq("Request replaced"), eq(1));
        inOrder.verify(mSessionManager).addSessionManagerListener(mProvider, CastSession.class);
        inOrder.verify(mSessionController).requestSessionLaunch();
        pendingCreateRouteRequestInfo = mProvider.getPendingCreateRouteRequestInfo();
        assertEquals(pendingCreateRouteRequestInfo.source, mockSource);
        assertEquals(pendingCreateRouteRequestInfo.sink.getId(), "other-cast-route");
        assertEquals(pendingCreateRouteRequestInfo.presentationId, "presentation-id-2");
        assertEquals(pendingCreateRouteRequestInfo.origin, "origin-2");
        assertEquals(pendingCreateRouteRequestInfo.tabId, 2);
        assertEquals(pendingCreateRouteRequestInfo.isIncognito, true);
        assertEquals(pendingCreateRouteRequestInfo.nativeRequestId, 2);
        assertEquals(
                pendingCreateRouteRequestInfo.routeInfo, mMediaRouterHelper.getOtherCastRoute());

        // A new request ends the current session if there's any.
        doReturn(true).when(mSessionController).isConnected();
        mProvider.createRoute("source-id", "cast-route", "presentation-id", "origin", 1, false, 1);
        inOrder.verify(mSessionController).endSession();
        inOrder.verify(mSessionManager).addSessionManagerListener(mProvider, CastSession.class);
        inOrder.verify(mSessionController).requestSessionLaunch();
    }

    @Test
    public void testCreateRoute_invalidRequest() {
        InOrder inOrder = inOrder(mSessionManager, mSessionController, mManager);
        MediaSource mockSource = mock(MediaSource.class);
        doReturn(mockSource).when(mProvider).getSourceFromId("source-id");

        // Invalid sink.
        mProvider.createRoute(
                "source-id", "invalid-route", "presentation-id", "origin", 1, false, 1);
        inOrder.verify(mManager).onRouteRequestError("No sink", 1);

        // Invalid source.
        doReturn(null).when(mProvider).getSourceFromId(anyString());
        mProvider.createRoute("source-id", "cast-route", "presentation-id", "origin", 1, false, 1);
        inOrder.verify(mManager).onRouteRequestError("Unsupported source URL", 1);
    }

    @Test
    public void testCloseRoute() {
        InOrder inOrder = inOrder(mSessionController);
        MediaRoute route = new MediaRoute("sink-id", "source-id", "presentation-id");

        // Normal case.
        mProvider.addRoute(route, "origin", 1, 1, false);
        doReturn(true).when(mSessionController).isConnected();
        mProvider.closeRoute(route.id);
        inOrder.verify(mSessionController).endSession();
        assertContainsRoutes(route);

        // Route doesn't exist.
        mProvider.closeRoute("other-route");
        inOrder.verify(mSessionController, never()).endSession();
        assertContainsRoutes(route);

        // No session.
        doReturn(false).when(mSessionController).isConnected();
        mProvider.closeRoute(route.id);
        inOrder.verify(mSessionController, never()).endSession();
        assertContainsRoutes(/* no route */);
    }

    @Test
    public void testDetachRoute() {
        InOrder inOrder = inOrder(mSessionController);
        MediaRoute route = new MediaRoute("sink-id", "source-id", "presentation-id");

        mProvider.addRoute(route, "origin", 1, 1, false);
        mProvider.detachRoute(route.id);
        assertContainsRoutes(/* no route */);
    }

    @Test
    public void testOnSessionStartFailed() {
        InOrder inOrder = inOrder(mProvider, mManager);
        MediaSource mockSource = mock(MediaSource.class);
        doReturn(mockSource).when(mProvider).getSourceFromId("source-id");

        // Request to create a session.
        mProvider.createRoute("source-id", "cast-route", "presentation-id", "origin", 1, false, 1);
        inOrder.verify(mManager, never()).onRouteRequestError(anyString(), anyInt());

        // Session start failed.
        mProvider.onSessionStartFailed(mCastSession, 1);

        inOrder.verify(mProvider).removeAllRoutes("Launch error");
        inOrder.verify(mManager).onRouteRequestError("Launch error", 1);
    }

    @Test
    public void testOnSessionStarted() {
        InOrder inOrder = inOrder(mSessionController);
        MediaSource mockSource = mock(MediaSource.class);
        doReturn(mockSource).when(mProvider).getSourceFromId("source-id");
        doReturn("source-id").when(mockSource).getSourceId();
        doReturn(mCastSession).when(mSessionManager).getCurrentCastSession();

        // Request to create a session.
        mProvider.createRoute("source-id", "cast-route", "presentation-id", "origin", 1, false, 1);

        // Session started.
        mProvider.onSessionStarted(mCastSession, "session-id");

        inOrder.verify(mSessionController).attachToCastSession(mCastSession);
        inOrder.verify(mSessionController).onSessionStarted();
        assertEquals(mProvider.mRoutes.size(), 1);

        MediaRoute route = (MediaRoute) (mProvider.mRoutes.values().toArray()[0]);
        assertEquals(route.sinkId, "cast-route");
        assertEquals(route.sourceId, "source-id");
        assertEquals(route.presentationId, "presentation-id");
        assertNull(mProvider.getPendingCreateRouteRequestInfo());
    }

    @Test
    public void testOnSessionStarted_nonCurrentSession() {
        CastSession otherCastSession = mock(CastSession.class);

        InOrder inOrder = inOrder(mSessionController);
        MediaSource mockSource = mock(MediaSource.class);
        doReturn(mockSource).when(mProvider).getSourceFromId("source-id");
        doReturn("source-id").when(mockSource).getSourceId();
        doReturn(otherCastSession).when(mSessionManager).getCurrentCastSession();

        // Request to create a session.
        mProvider.createRoute("source-id", "cast-route", "presentation-id", "origin", 1, false, 1);

        // Session started.
        mProvider.onSessionStarted(mCastSession, "session-id");

        inOrder.verify(mSessionController, never()).attachToCastSession(mCastSession);
        inOrder.verify(mSessionController, never()).onSessionStarted();
        assertTrue(mProvider.mRoutes.isEmpty());
    }

    @Test
    public void testOnSessionStarted_twiceForSameSession() {
        InOrder inOrder = inOrder(mSessionController);
        MediaSource mockSource = mock(MediaSource.class);
        doReturn(mockSource).when(mProvider).getSourceFromId("source-id");
        doReturn("source-id").when(mockSource).getSourceId();
        doReturn(mCastSession).when(mSessionManager).getCurrentCastSession();

        // Request to create a session.
        mProvider.createRoute("source-id", "cast-route", "presentation-id", "origin", 1, false, 1);

        // Session started.
        mProvider.onSessionStarted(mCastSession, "session-id");

        inOrder.verify(mSessionController).attachToCastSession(mCastSession);
        inOrder.verify(mSessionController).onSessionStarted();
        assertEquals(mProvider.mRoutes.size(), 1);

        MediaRoute route = (MediaRoute) (mProvider.mRoutes.values().toArray()[0]);
        assertEquals(route.sinkId, "cast-route");
        assertEquals(route.sourceId, "source-id");
        assertEquals(route.presentationId, "presentation-id");
        assertNull(mProvider.getPendingCreateRouteRequestInfo());

        // Same session started for the second time.
        mProvider.onSessionStarted(mCastSession, "session-id");

        inOrder.verify(mSessionController, never()).attachToCastSession(mCastSession);
        inOrder.verify(mSessionController, never()).onSessionStarted();
        assertEquals(mProvider.mRoutes.size(), 1);

        route = (MediaRoute) (mProvider.mRoutes.values().toArray()[0]);
        assertEquals(route.sinkId, "cast-route");
        assertEquals(route.sourceId, "source-id");
        assertEquals(route.presentationId, "presentation-id");
        assertNull(mProvider.getPendingCreateRouteRequestInfo());
    }

    @Test
    public void testOnSessionResumed() {
        mProvider.onSessionResumed(mCastSession, true);

        verify(mSessionController).attachToCastSession(mCastSession);
    }

    @Test
    public void testOnSessionEnding() {
        InOrder inOrder = inOrder(
                mSessionController, mProvider, mSessionManager, mMediaRouterHelper.getShadowImpl());

        mProvider.onSessionEnding(mCastSession);

        verifySessionEnd(inOrder);
    }

    @Test
    public void testOnSessionEnding_hasPendingRequest() {
        // If there's a pending request, then the session ending event comes from ending the
        // previous session.
        InOrder inOrder = inOrder(
                mSessionController, mProvider, mSessionManager, mMediaRouterHelper.getShadowImpl());

        MediaSource mockSource = mock(MediaSource.class);
        doReturn(mockSource).when(mProvider).getSourceFromId("source-id");
        doReturn("source-id").when(mockSource).getSourceId();

        // Request to create a session.
        mProvider.createRoute("source-id", "cast-route", "presentation-id", "origin", 1, false, 1);

        // Simulate a session is ending.
        mProvider.onSessionEnding(mCastSession);

        verifySessionNeverEnd(inOrder);
    }

    @Test
    public void testOnSessionEnded() {
        InOrder inOrder = inOrder(
                mSessionController, mProvider, mSessionManager, mMediaRouterHelper.getShadowImpl());

        mProvider.onSessionEnded(mCastSession, 0);

        verifySessionEnd(inOrder);
    }

    @Test
    public void testOnSessionEnded_hasPendingRequest() {
        // If there's a pending request, then the session ending event comes from ending the
        // previous session.
        InOrder inOrder = inOrder(
                mSessionController, mProvider, mSessionManager, mMediaRouterHelper.getShadowImpl());

        MediaSource mockSource = mock(MediaSource.class);
        doReturn(mockSource).when(mProvider).getSourceFromId("source-id");
        doReturn("source-id").when(mockSource).getSourceId();

        // Request to create a session.
        mProvider.createRoute("source-id", "cast-route", "presentation-id", "origin", 1, false, 1);

        // Simulate a session has ended.
        mProvider.onSessionEnded(mCastSession, 0);

        verifySessionNeverEnd(inOrder);
    }

    @Test
    public void testOnSessionSuspended() {
        mProvider.onSessionSuspended(mCastSession, 0);

        verify(mSessionController).detachFromCastSession();
    }

    @Test
    public void testAddRoute() {
        InOrder inOrder = inOrder(mManager);

        MediaRoute route1 = new MediaRoute("sink-id-1", "source-id-1", "presentation-id-1");
        MediaRoute route2 = new MediaRoute("sink-id-2", "source-id-2", "presentation-id-2");

        // Add the first route.
        mProvider.addRoute(route1, "origin-1", 1, 1, false);
        assertContainsRoutes(route1);
        inOrder.verify(mManager).onRouteCreated(route1.id, "sink-id-1", 1, mProvider, false);

        // Add the second route.
        mProvider.addRoute(route2, "origin-2", 2, 2, true);
        assertContainsRoutes(route1, route2);
        inOrder.verify(mManager).onRouteCreated(route2.id, "sink-id-2", 2, mProvider, true);

        // Add a duplicate route. This should never happen but the manager should be notified just
        // to be safe.
        mProvider.addRoute(route1, "origin-1", 3, 3, false);
        assertContainsRoutes(route1, route2);
        inOrder.verify(mManager).onRouteCreated(route1.id, "sink-id-1", 3, mProvider, false);
    }

    @Test
    public void testRemoveRoute() {
        InOrder inOrder = inOrder(mManager);

        MediaRoute route1 = new MediaRoute("sink-id-1", "source-id-1", "presentation-id-1");
        MediaRoute route2 = new MediaRoute("sink-id-2", "source-id-2", "presentation-id-2");

        mProvider.addRoute(route1, "origin-1", 1, 1, false);
        mProvider.addRoute(route2, "origin-2", 2, 2, true);

        // Remove the first route.
        mProvider.removeRoute(route1.id, "error 1");
        assertContainsRoutes(route2);
        inOrder.verify(mManager).onRouteClosed(route1.id, "error 1");

        // Remove the second route.
        mProvider.removeRoute(route2.id, "error 2");
        assertContainsRoutes(/* no route */);
        inOrder.verify(mManager).onRouteClosed(route2.id, "error 2");

        // Remove a duplicate route. This should never happen but the manager should be notified
        // just to be safe.
        mProvider.removeRoute(route1.id, "error 3");
        assertContainsRoutes(/* no route */);
        inOrder.verify(mManager).onRouteClosed(route1.id, "error 3");
    }

    @Test
    public void testRemoveAllRoutes() {
        MediaRoute route1 = new MediaRoute("sink-id-1", "source-id-1", "presentation-id-1");
        MediaRoute route2 = new MediaRoute("sink-id-2", "source-id-2", "presentation-id-2");

        mProvider.addRoute(route1, "origin-1", 1, 1, false);
        mProvider.addRoute(route2, "origin-2", 2, 2, true);

        mProvider.removeAllRoutes("error");

        assertContainsRoutes(/* no route */);
        verify(mManager).onRouteClosed(route1.id, "error");
        verify(mManager).onRouteClosed(route2.id, "error");
    }

    @Test
    public void testTerminateAllRoutes() {
        MediaRoute route1 = new MediaRoute("sink-id-1", "source-id-1", "presentation-id-1");
        MediaRoute route2 = new MediaRoute("sink-id-2", "source-id-2", "presentation-id-2");

        mProvider.addRoute(route1, "origin-1", 1, 1, false);
        mProvider.addRoute(route2, "origin-2", 2, 2, true);

        mProvider.terminateAllRoutes();

        assertContainsRoutes(/* no route */);
        verify(mManager).onRouteTerminated(route1.id);
        verify(mManager).onRouteTerminated(route2.id);
        verify(mManager, never()).onRouteClosed(eq(route1.id), anyString());
    }

    private void prepareMediaSource(
            MediaSource source, MediaRouteSelector selector, String sourceId, String appId) {
        doReturn(source).when(mProvider).getSourceFromId(sourceId);
        doReturn(appId).when(source).getApplicationId();
        doReturn(selector).when(source).buildRouteSelector();
    }

    private void verifySessionEnd(InOrder inOrder) {
        inOrder.verify(mSessionController).onSessionEnded();
        inOrder.verify(mSessionController).detachFromCastSession();
        inOrder.verify(mMediaRouterHelper.getShadowImpl())
                .selectRoute(mMediaRouterHelper.getDefaultRoute());
        inOrder.verify(mProvider).terminateAllRoutes();
        inOrder.verify(mSessionManager).removeSessionManagerListener(mProvider, CastSession.class);
    }

    private void verifySessionNeverEnd(InOrder inOrder) {
        inOrder.verify(mSessionController, never()).onSessionEnded();
        inOrder.verify(mSessionController, never()).detachFromCastSession();
        inOrder.verify(mMediaRouterHelper.getShadowImpl(), never())
                .selectRoute(mMediaRouterHelper.getDefaultRoute());
        inOrder.verify(mProvider, never()).terminateAllRoutes();
        inOrder.verify(mSessionManager, never())
                .removeSessionManagerListener(mProvider, CastSession.class);
    }

    private void assertContainsRoutes(MediaRoute... routes) {
        assertEquals(mProvider.mRoutes.size(), routes.length);

        for (MediaRoute route : routes) {
            assertEquals(mProvider.mRoutes.get(route.id), route);
        }
    }

    private static class TestMRP extends CafBaseMediaRouteProvider {
        public TestMRP(MediaRouter androidMediaRouter, MediaRouteManager manager) {
            super(androidMediaRouter, manager);
        }

        @Override
        public MediaSource getSourceFromId(@NonNull String sourceId) {
            return null;
        }

        @Override
        public BaseSessionController sessionController() {
            return null;
        }

        @Override
        public void sendStringMessage(String routeId, String message) {}

        @Override
        public void joinRoute(String routeId, String presentationId, String origin, int tabId,
                int nativeRequestId) {}
    }
}
