// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.support.v7.media.MediaRouteSelector;
import android.support.v7.media.MediaRouter;

import com.google.android.gms.cast.CastDevice;
import com.google.android.gms.cast.CastMediaControlIntent;
import com.google.android.gms.cast.framework.CastContext;
import com.google.android.gms.cast.framework.CastSession;
import com.google.android.gms.cast.framework.SessionManager;
import com.google.android.gms.cast.framework.media.RemoteMediaClient;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.media.router.CastSessionUtil;
import org.chromium.chrome.browser.media.router.MediaSink;
import org.chromium.chrome.browser.media.router.MediaSource;

import java.util.ArrayList;
import java.util.List;

/**
 * Robolectric tests for BaseSessionController.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowMediaRouter.class, ShadowCastContext.class})
public class BaseSessionControllerTest {
    private static final String PRESENTATION_ID = "presentation-id";
    private static final String ORIGIN = "https://example.com/";
    private static final int TAB_ID = 1;
    private static final String APP_ID = "12345678";

    @Mock
    private CastDevice mCastDevice;
    @Mock
    private CafBaseMediaRouteProvider mProvider;
    @Mock
    private BaseNotificationController mNotificationController;
    @Mock
    private MediaSource mSource;
    @Mock
    private MediaSink mSink;
    @Mock
    private CastContext mCastContext;
    @Mock
    private CastSession mCastSession;
    @Mock
    private SessionManager mSessionManager;
    @Mock
    private RemoteMediaClient mRemoteMediaClient;
    private BaseSessionController mController;
    private CreateRouteRequestInfo mRequestInfo;
    private MediaRouterTestHelper mMediaRouterHelper;
    private MediaRouteSelector mMediaRouteSelector;
    private Context mContext;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        mMediaRouterHelper = new MediaRouterTestHelper();
        ShadowCastContext.setInstance(mCastContext);
        mMediaRouteSelector =
                new MediaRouteSelector.Builder()
                        .addControlCategory(CastMediaControlIntent.categoryForCast(APP_ID))
                        .build();
        mController = new TestSessionController(mProvider, mNotificationController);
        mController.addCallback(mNotificationController);
        mRequestInfo = new CreateRouteRequestInfo(mSource, mSink, PRESENTATION_ID, ORIGIN, TAB_ID,
                false, 1, mMediaRouterHelper.getCastRoute());

        doReturn(mSessionManager).when(mCastContext).getSessionManager();
        doReturn(mRemoteMediaClient).when(mCastSession).getRemoteMediaClient();
        doReturn(true).when(mCastSession).isConnected();
        doReturn(mCastDevice).when(mCastSession).getCastDevice();
        doReturn(mMediaRouteSelector).when(mSource).buildRouteSelector();
        doReturn(APP_ID).when(mSource).getApplicationId();
        doReturn(mRequestInfo).when(mProvider).getPendingCreateRouteRequestInfo();
        doReturn(MediaRouter.getInstance(mContext)).when(mProvider).getAndroidMediaRouter();
    }

    @Test
    public void testInitialState() {
        assertFalse(mController.isConnected());
        assertNull(mController.getSession());
        assertNull(mController.getRemoteMediaClient());
        assertNull(mController.getRouteCreationInfo());
    }

    @Test
    public void testRequestSessionLaunch() {
        mController.requestSessionLaunch();
        verify(mCastContext).setReceiverApplicationId(APP_ID);
        verify(mMediaRouterHelper.getCastRoute()).select();
        assertSame(mRequestInfo, mController.getRouteCreationInfo());
    }

    @Test
    public void testEndSession() {
        mController.endSession();
        verify(mSessionManager).endCurrentSession(/* stopCasting= */ true);
        verify(mCastContext).setReceiverApplicationId(null);
    }

    @Test
    public void testSessionAttachment() {
        InOrder inOrder = inOrder(mRemoteMediaClient);

        mController.attachToCastSession(mCastSession);
        assertSame(mController.getSession(), mCastSession);
        inOrder.verify(mRemoteMediaClient).registerCallback(any(RemoteMediaClient.Callback.class));

        mController.detachFromCastSession();
        assertNull(mController.getSession());
        inOrder.verify(mRemoteMediaClient)
                .unregisterCallback(any(RemoteMediaClient.Callback.class));

        // Attaching/detaching while the session is disconnected.
        doReturn(false).when(mCastSession).isConnected();
        mController.attachToCastSession(mCastSession);
        assertSame(mController.getSession(), mCastSession);
        inOrder.verify(mRemoteMediaClient).registerCallback(any(RemoteMediaClient.Callback.class));

        mController.detachFromCastSession();
        assertNull(mController.getSession());
        inOrder.verify(mRemoteMediaClient)
                .unregisterCallback(any(RemoteMediaClient.Callback.class));
    }

    @Test
    public void testOnSessionStarted() {
        mController.attachToCastSession(mCastSession);
        mController.onSessionStarted();
        verify(mNotificationController).onSessionStarted();

        mController.onSessionEnded();
        verify(mNotificationController).onSessionEnded();
    }

    @Test
    public void testSessionLifecyleNotNotifiedAfterCallbackRemoved() {
        mController.removeCallback(mNotificationController);

        mController.attachToCastSession(mCastSession);
        mController.onSessionStarted();
        verify(mNotificationController, never()).onSessionStarted();

        mController.onSessionEnded();
        verify(mNotificationController, never()).onSessionEnded();
    }

    @Test
    public void testGetCapabilities() {
        mController.attachToCastSession(mCastSession);

        // Test that nothing is supported.
        doReturn(false).when(mCastDevice).hasCapability(anyInt());
        assertEquals(mController.getCapabilities(), new ArrayList<>());

        // Test that everything is supported.
        doReturn(true).when(mCastDevice).hasCapability(anyInt());
        List<String> capabilities = mController.getCapabilities();
        assertEquals(capabilities.size(), 4);
        assertTrue(capabilities.contains("audio_in"));
        assertTrue(capabilities.contains("audio_out"));
        assertTrue(capabilities.contains("video_in"));
        assertTrue(capabilities.contains("video_out"));
    }

    @Test
    public void testOnMessageReceived() {
        mController.attachToCastSession(mCastSession);

        // Non-media namespace should be no-op.
        mController.onMessageReceived(mCastDevice, "namespace", "message");
        verify(mRemoteMediaClient, never())
                .onMessageReceived(any(CastDevice.class), anyString(), anyString());

        // Media namespaces should be forwarded to RemoteMediaClient.
        mController.onMessageReceived(mCastDevice, CastSessionUtil.MEDIA_NAMESPACE, "message");
        verify(mRemoteMediaClient)
                .onMessageReceived(mCastDevice, CastSessionUtil.MEDIA_NAMESPACE, "message");
    }

    @Test
    public void testOnStatusChangeUpdatesNotificationController() {
        mController.attachToCastSession(mCastSession);

        ArgumentCaptor<RemoteMediaClient.Callback> callbackCaptor =
                ArgumentCaptor.forClass(RemoteMediaClient.Callback.class);
        verify(mRemoteMediaClient).registerCallback(callbackCaptor.capture());

        callbackCaptor.getValue().onStatusUpdated();
        verify(mNotificationController).onStatusUpdated();

        callbackCaptor.getValue().onMetadataUpdated();
        verify(mNotificationController).onMetadataUpdated();
    }

    private static class TestSessionController extends BaseSessionController {
        public BaseNotificationController mNotificationController;

        public TestSessionController(CafBaseMediaRouteProvider provider,
                BaseNotificationController notificationController) {
            super(provider);
            mNotificationController = notificationController;
        }

        @Override
        public BaseNotificationController getNotificationController() {
            return mNotificationController;
        }
    }
}
