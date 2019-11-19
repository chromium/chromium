// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyDouble;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import com.google.android.gms.cast.ApplicationMetadata;
import com.google.android.gms.cast.CastDevice;
import com.google.android.gms.cast.framework.CastSession;
import com.google.android.gms.common.api.Status;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.media.router.CastSessionUtil;
import org.chromium.chrome.browser.media.router.ClientRecord;
import org.chromium.chrome.browser.media.router.JSONTestUtils.JSONObjectLike;
import org.chromium.chrome.browser.media.router.JSONTestUtils.JSONStringLike;
import org.chromium.chrome.browser.media.router.MediaSink;
import org.chromium.chrome.browser.media.router.caf.CafMessageHandler.RequestRecord;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Robolectric tests for CastSession.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CafMessageHandlerTest {
    private static final String TAG = "MediaRouter";

    private static final String SESSION_ID = "SESSION_ID";
    private static final String INVALID_SESSION_ID = "INVALID_SESSION_ID";
    private static final String CLIENT_ID1 = "client-id-1";
    private static final String CLIENT_ID2 = "client-id-2";
    private static final String INVALID_CLIENT_ID = "xxxxxxxxxxxxxxxxx";
    private static final String NAMESPACE1 = "namespace1";
    private static final String NAMESPACE2 = "namespace2";
    private static final String MEDIA_NAMESPACE = CastSessionUtil.MEDIA_NAMESPACE;
    private static final int SEQUENCE_NUMBER1 = 1;
    private static final int SEQUENCE_NUMBER2 = 2;
    private static final int REQUEST_ID1 = 1;
    private static final int REQUEST_ID2 = 2;
    private static final int VOID_SEQUENCE_NUMBER = CafMessageHandler.VOID_SEQUENCE_NUMBER;
    private CafMediaRouteProvider mRouteProvider;
    @Mock
    private CastSession mSession;
    @Mock
    private CastSessionController mSessionController;
    private ClientRecord mClientRecord1;
    private ClientRecord mClientRecord2;
    private Map<String, ClientRecord> mClientRecordMap;
    private CafMessageHandler mMessageHandler;
    private int mNumStopApplicationCalled;

    private interface CheckedRunnable { void run(); }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mRouteProvider = mock(CafMediaRouteProvider.class);
        mClientRecord1 =
                new ClientRecord("route-id-1", CLIENT_ID1, "app-id", "auto-join", "origin", 1);
        mClientRecord2 =
                new ClientRecord("route-id-2", CLIENT_ID2, "app-id", "auto-join", "origin2", 1);

        mMessageHandler = spy(new CafMessageHandler(mRouteProvider, mSessionController));

        doReturn(SESSION_ID).when(mSessionController).getSessionId();
        doReturn(true)
                .when(mMessageHandler)
                .sendStringCastMessage(anyString(), anyString(), anyString(), anyInt());
        doReturn(mSession).when(mSessionController).getSession();
        doReturn(true).when(mSessionController).isConnected();
        doReturn(SESSION_ID).when(mSessionController).getSessionId();

        mClientRecordMap = new HashMap<>();

        mClientRecordMap.put(CLIENT_ID1, mClientRecord1);
        mClientRecordMap.put(CLIENT_ID2, mClientRecord2);
        doReturn(mClientRecordMap).when(mRouteProvider).getClientIdToRecords();
        doNothing().when(mRouteProvider).sendMessageToClient(anyString(), anyString());
    }

    void setUpForAppMessageTest() throws JSONException {
        List<String> namespaces = new ArrayList<String>();
        namespaces.add(NAMESPACE1);
        doReturn(namespaces).when(mSessionController).getNamespaces();
        doReturn(true)
                .when(mMessageHandler)
                .sendJsonCastMessage(any(JSONObject.class), anyString(), anyString(), anyInt());
    }

    @Test
    public void testOnSessionStarted() {
        doReturn("session_message").when(mMessageHandler).buildSessionMessage();
        // The call in setUp() actually only sets the session. This call will notify the clients
        // that have sent "client_connect"
        mClientRecord1.isConnected = true;

        mMessageHandler.onSessionStarted();

        verify(mMessageHandler)
                .sendEnclosedMessageToClient(
                        CLIENT_ID1, "new_session", "session_message", VOID_SEQUENCE_NUMBER);
        verify(mMessageHandler, never())
                .sendEnclosedMessageToClient(eq(CLIENT_ID2), anyString(), anyString(), anyInt());
    }

    @Test
    public void testHandleClientConnectMessage_sessionConnected() throws JSONException {
        doReturn("session_message").when(mMessageHandler).buildSessionMessage();
        mClientRecord1.isConnected = false;
        JSONObject message = new JSONObject();
        message.put("type", "client_connect");
        message.put("clientId", mClientRecord1.clientId);

        assertTrue(mMessageHandler.handleMessageFromClient(message.toString()));

        assertTrue(mClientRecord1.isConnected);
        verify(mMessageHandler)
                .sendEnclosedMessageToClient(mClientRecord1.clientId, "new_session",
                        "session_message", VOID_SEQUENCE_NUMBER);
        verify(mRouteProvider).flushPendingMessagesToClient(mClientRecord1);
    }

    @Test
    public void testHandleClientConnectMessage_sessionNotConnected() throws JSONException {
        doReturn("session_message").when(mMessageHandler).buildSessionMessage();
        doReturn(false).when(mSessionController).isConnected();
        mClientRecord1.isConnected = false;
        JSONObject message = new JSONObject();
        message.put("type", "client_connect");
        message.put("clientId", mClientRecord1.clientId);

        assertTrue(mMessageHandler.handleMessageFromClient(message.toString()));

        assertTrue(mClientRecord1.isConnected);
        verify(mMessageHandler, never())
                .sendEnclosedMessageToClient(anyString(), anyString(), anyString(), anyInt());
        verify(mRouteProvider).flushPendingMessagesToClient(mClientRecord1);
    }

    @Test
    public void testHandleClientConnectMessage_noClientRecord() throws JSONException {
        doReturn("session_message").when(mMessageHandler).buildSessionMessage();
        JSONObject message = new JSONObject();
        message.put("type", "client_connect");
        message.put("clientId", "other-client-id");

        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));

        verify(mMessageHandler, never())
                .sendEnclosedMessageToClient(anyString(), anyString(), anyString(), anyInt());
        verify(mRouteProvider, never()).flushPendingMessagesToClient(any(ClientRecord.class));
    }

    @Test
    public void testHandleClientConnectMessage_noClientId() throws JSONException {
        doReturn("session_message").when(mMessageHandler).buildSessionMessage();
        JSONObject message = new JSONObject();
        message.put("type", "client_connect");

        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));

        verify(mMessageHandler, never())
                .sendEnclosedMessageToClient(anyString(), anyString(), anyString(), anyInt());
        verify(mRouteProvider, never()).flushPendingMessagesToClient(any(ClientRecord.class));
    }

    @Test
    public void testHandleClientDisconnectMessage() throws JSONException {
        JSONObject message = new JSONObject();
        message.put("type", "client_disconnect");
        message.put("clientId", mClientRecord1.clientId);

        assertTrue(mMessageHandler.handleMessageFromClient(message.toString()));

        verify(mRouteProvider).removeRoute("route-id-1", null);
    }

    @Test
    public void testHandleClientDisconnectMessage_noClientRecord() throws JSONException {
        JSONObject message = new JSONObject();
        message.put("type", "client_disconnect");
        message.put("clientId", "other-client-id");

        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));

        verify(mRouteProvider, never()).removeRoute(anyString(), anyString());
    }

    @Test
    public void testHandleClientDisconnectMessage_noClientId() throws JSONException {
        JSONObject message = new JSONObject();
        message.put("type", "client_disconnect");

        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));

        verify(mRouteProvider, never()).removeRoute(anyString(), anyString());
    }

    @Test
    public void testHandleClientLeaveSessionMessage() throws JSONException {
        JSONObject message = buildLeaveSessionMessage(mClientRecord1.clientId, SESSION_ID, 12345);
        ArgumentCaptor<String> messageCaptor = ArgumentCaptor.forClass(String.class);

        assertTrue(mMessageHandler.handleMessageFromClient(message.toString()));

        verify(mRouteProvider)
                .sendMessageToClient(eq(mClientRecord1.clientId), messageCaptor.capture());
        JSONObject capturedMessage = new JSONObject(messageCaptor.getValue());
        verifySimpleSessionMessage(
                capturedMessage, "leave_session", 12345, mClientRecord1.clientId);
    }

    @Test
    public void testHandleClientLeaveSessionMessage_noSequenceNumber() throws JSONException {
        JSONObject message = buildLeaveSessionMessage(mClientRecord1.clientId, SESSION_ID, 12345);
        message.remove("sequenceNumber");
        ArgumentCaptor<String> messageCaptor = ArgumentCaptor.forClass(String.class);

        assertTrue(mMessageHandler.handleMessageFromClient(message.toString()));

        verify(mRouteProvider)
                .sendMessageToClient(eq(mClientRecord1.clientId), messageCaptor.capture());
        JSONObject capturedMessage = new JSONObject(messageCaptor.getValue());
        verifySimpleSessionMessage(
                capturedMessage, "leave_session", VOID_SEQUENCE_NUMBER, mClientRecord1.clientId);
    }

    @Test
    public void testHandleClientLeaveSessionMessage_noClientRecord() throws JSONException {
        JSONObject message = buildLeaveSessionMessage("other-client-id", SESSION_ID, 12345);

        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));

        verify(mRouteProvider, never()).sendMessageToClient(anyString(), anyString());
    }

    @Test
    public void testHandleClientLeaveSessionMessage_wrongSessionId() throws JSONException {
        JSONObject message =
                buildLeaveSessionMessage(mClientRecord1.clientId, "other-session-id", 12345);

        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));

        verify(mRouteProvider, never()).sendMessageToClient(anyString(), anyString());
    }

    @Test
    public void testHandleClientLeaveSessionMessage_sessionNotConnected() throws JSONException {
        doReturn(false).when(mSessionController).isConnected();
        JSONObject message = buildLeaveSessionMessage(mClientRecord1.clientId, SESSION_ID, 12345);

        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));

        verify(mRouteProvider, never()).sendMessageToClient(anyString(), anyString());
    }

    @Test
    public void testHandleClientLeaveSessionMessage_noClientId() throws JSONException {
        JSONObject message = buildLeaveSessionMessage(mClientRecord1.clientId, SESSION_ID, 12345);
        message.remove("clientId");

        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));

        verify(mRouteProvider, never()).sendMessageToClient(anyString(), anyString());
    }

    @Test
    public void testHandleClientLeaveSessionMessage_removeRoutes_tabAndOriginScoped()
            throws JSONException {
        mClientRecordMap.clear();

        prepareClientRecord(
                "route-id-1", "client-id-1", "app-id", "tab_and_origin_scoped", "origin", 1);
        prepareClientRecord("route-id-2", "client-id-2", "app-id", "auto-join", "origin", 1);
        prepareClientRecord("route-id-3", "client-id-3", "app-id", "auto-join", "origin2", 1);
        prepareClientRecord("route-id-4", "client-id-4", "app-id", "auto-join", "origin", 2);

        JSONObject message = buildLeaveSessionMessage("client-id-1", SESSION_ID, 12345);

        assertTrue(mMessageHandler.handleMessageFromClient(message.toString()));

        // Only routes with same origin and tab ID will be removed.
        verify(mRouteProvider).removeRoute("route-id-1", null);
        verify(mRouteProvider).removeRoute("route-id-2", null);
        verify(mRouteProvider, never()).removeRoute("route-id-3", null);
        verify(mRouteProvider, never()).removeRoute("route-id-4", null);
    }

    @Test
    public void testHandleClientLeaveSessionMessage_removeRoutes_originScoped()
            throws JSONException {
        prepareClientRecord("route-id-1", "client-id-1", "app-id", "origin_scoped", "origin", 1);
        prepareClientRecord("route-id-2", "client-id-2", "app-id", "auto-join", "origin", 1);
        prepareClientRecord("route-id-3", "client-id-3", "app-id", "auto-join", "origin2", 1);
        prepareClientRecord("route-id-4", "client-id-4", "app-id", "auto-join", "origin", 2);

        JSONObject message = buildLeaveSessionMessage("client-id-1", SESSION_ID, 12345);

        assertTrue(mMessageHandler.handleMessageFromClient(message.toString()));

        // Only routes with same origin will be removed.
        verify(mRouteProvider).removeRoute("route-id-1", null);
        verify(mRouteProvider).removeRoute("route-id-2", null);
        verify(mRouteProvider, never()).removeRoute("route-id-3", null);
        verify(mRouteProvider).removeRoute("route-id-4", null);
    }

    @Test
    public void testHandleClientLeaveSessionMessage_removeRoutes_otherScoped()
            throws JSONException {
        prepareClientRecord("route-id-1", "client-id-1", "app-id", "other_scoped", "origin", 1);
        prepareClientRecord("route-id-2", "client-id-2", "app-id", "auto-join", "origin", 1);
        prepareClientRecord("route-id-3", "client-id-3", "app-id", "auto-join", "origin2", 1);
        prepareClientRecord("route-id-4", "client-id-4", "app-id", "auto-join", "origin", 2);

        JSONObject message = buildLeaveSessionMessage("client-id-1", SESSION_ID, 12345);

        assertTrue(mMessageHandler.handleMessageFromClient(message.toString()));

        // No route should be removed.
        verify(mRouteProvider, never()).removeRoute(anyString(), anyString());
    }

    @Test
    public void testHandleSessionMessageOfV2MessageType() throws JSONException {
        doReturn(true).when(mMessageHandler).handleCastV2MessageFromClient(any(JSONObject.class));

        JSONObject message = new JSONObject();
        message.put("type", "v2_message");
        assertTrue(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler).handleCastV2MessageFromClient(argThat(new JSONObjectLike(message)));
    }

    @Test
    public void testHandleSessionMessageOfAppMessageType() throws JSONException {
        doReturn(true).when(mMessageHandler).handleAppMessageFromClient(any(JSONObject.class));

        JSONObject message = new JSONObject();
        message.put("type", "app_message");
        assertTrue(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler).handleAppMessageFromClient(argThat(new JSONObjectLike(message)));
    }

    @Test
    public void testHandleSessionMessageOfUnsupportedType() throws JSONException {
        doReturn(true).when(mMessageHandler).handleCastV2MessageFromClient(any(JSONObject.class));

        JSONObject message = new JSONObject();
        message.put("type", "unsupported");
        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler, never()).handleCastV2MessageFromClient(any(JSONObject.class));
        verify(mMessageHandler, never()).handleAppMessageFromClient(any(JSONObject.class));
    }

    @Test
    public void testCastV2MessageWithWrongTypeInnerMessage() throws JSONException {
        org.robolectric.shadows.ShadowLog.stream = System.out;
        JSONObject innerMessage = new JSONObject().put("type", "STOP");
        final JSONObject message = buildCastV2Message(CLIENT_ID1, innerMessage);
        // Replace the inner JSON message with string.
        message.put("message", "wrong type inner message");
        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler, never()).handleStopMessage(anyString(), anyInt());
        verify(mMessageHandler, never())
                .handleVolumeMessage(any(JSONObject.class), anyString(), anyInt());
        verify(mMessageHandler, never())
                .sendJsonCastMessage(any(JSONObject.class), anyString(), anyString(), anyInt());
    }

    @Test
    public void testCastV2MessageOfStopType() throws JSONException {
        JSONObject innerMessage = new JSONObject().put("type", "STOP");
        JSONObject message = buildCastV2Message(CLIENT_ID1, innerMessage);
        assertTrue(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler).handleStopMessage(eq(CLIENT_ID1), eq(SEQUENCE_NUMBER1));
    }

    @Test
    public void testCastV2MessageofSetVolumeTypeShouldWait() throws Exception {
        doReturn(true).when(mSession).isMute();
        JSONObject innerMessage =
                new JSONObject()
                        .put("type", "SET_VOLUME")
                        .put("volume",
                                new JSONObject().put("level", (double) 1).put("muted", false));
        JSONObject message = buildCastV2Message(CLIENT_ID1, innerMessage);
        assertTrue(mMessageHandler.handleMessageFromClient(message.toString()));
        JSONObject volumeMessage = innerMessage.getJSONObject("volume");
        verify(mSession).setMute(false);
        verify(mSession).setVolume(1.0);
        verify(mMessageHandler)
                .handleVolumeMessage(
                        argThat(new JSONObjectLike(innerMessage.getJSONObject("volume"))),
                        eq(CLIENT_ID1), eq(SEQUENCE_NUMBER1));
        assertEquals(mMessageHandler.getVolumeRequestsForTest().size(), 1);
    }

    @Test
    public void testCastV2MessageofSetVolumeTypeShouldNotWait() throws Exception {
        doReturn(false).when(mSession).isMute();
        doReturn(1.0).when(mSession).getVolume();
        JSONObject innerMessage =
                new JSONObject()
                        .put("type", "SET_VOLUME")
                        .put("volume",
                                new JSONObject().put("level", (double) 1).put("muted", false));
        JSONObject message = buildCastV2Message(CLIENT_ID1, innerMessage);
        assertTrue(mMessageHandler.handleMessageFromClient(message.toString()));
        JSONObject volumeMessage = innerMessage.getJSONObject("volume");
        verify(mSession, never()).setMute(anyBoolean());
        verify(mSession, never()).setVolume(anyDouble());
        verify(mMessageHandler)
                .handleVolumeMessage(
                        argThat(new JSONObjectLike(innerMessage.getJSONObject("volume"))),
                        eq(CLIENT_ID1), eq(SEQUENCE_NUMBER1));
        assertEquals(mMessageHandler.getVolumeRequestsForTest().size(), 0);
    }

    @Test
    public void testCastV2MessageofSetVolumeTypeWithNullVolumeMessage() throws JSONException {
        JSONObject innerMessage = new JSONObject().put("type", "SET_VOLUME");
        final JSONObject message = buildCastV2Message(CLIENT_ID1, innerMessage);
        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler, never())
                .handleVolumeMessage(any(JSONObject.class), anyString(), anyInt());
    }

    @Test
    public void testCastV2MessageofSetVolumeTypeWithWrongTypeVolumeMessage() throws JSONException {
        JSONObject innerMessage = new JSONObject()
                                          .put("type", "SET_VOLUME")
                                          .put("volume", "wrong type volume message");
        final JSONObject message = buildCastV2Message(CLIENT_ID1, innerMessage);
        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler, never())
                .handleVolumeMessage(any(JSONObject.class), anyString(), anyInt());
    }

    @Test
    public void testCastV2MessageOfMediaMessageType() throws JSONException {
        doReturn(true)
                .when(mMessageHandler)
                .sendJsonCastMessage(any(JSONObject.class), anyString(), anyString(), anyInt());
        for (String messageType : CafMessageHandler.getMediaMessageTypesForTest()) {
            // TODO(zqzhang): SET_VOLUME and STOP should not reach here?
            if ("MEDIA_SET_VOLUME".equals(messageType) || "STOP_MEDIA".equals(messageType)) {
                continue;
            }
            JSONObject innerMessage = new JSONObject().put("type", messageType);
            JSONObject message = buildCastV2Message(CLIENT_ID1, innerMessage);
            assertTrue(mMessageHandler.handleMessageFromClient(message.toString()));

            JSONObject expected = new JSONObject();
            if (CafMessageHandler.getMediaOverloadedMessageTypesForTest().containsKey(
                        messageType)) {
                expected.put("type",
                        CafMessageHandler.getMediaOverloadedMessageTypesForTest().get(messageType));
            } else {
                expected.put("type", messageType);
            }
            verify(mMessageHandler)
                    .sendJsonCastMessage(argThat(new JSONObjectLike(expected)),
                            eq(CastSessionUtil.MEDIA_NAMESPACE), eq(CLIENT_ID1),
                            eq(SEQUENCE_NUMBER1));
        }
    }

    @Test
    public void testCastV2MessageWithNullSequenceNumber() throws JSONException {
        JSONObject innerMessage = new JSONObject().put("type", "STOP");
        JSONObject message = buildCastV2Message(CLIENT_ID1, innerMessage);
        message.remove("sequenceNumber");
        assertTrue(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler).handleStopMessage(eq(CLIENT_ID1), eq(VOID_SEQUENCE_NUMBER));
    }

    @Test
    public void testHandleStopMessage() {
        InOrder inOrder = inOrder(mSessionController);
        assertEquals(0, mMessageHandler.getStopRequestsForTest().size());
        mMessageHandler.handleStopMessage(CLIENT_ID1, SEQUENCE_NUMBER1);
        assertEquals(1, mMessageHandler.getStopRequestsForTest().get(CLIENT_ID1).size(), 1);
        inOrder.verify(mSessionController).endSession();
        mMessageHandler.handleStopMessage(CLIENT_ID1, SEQUENCE_NUMBER2);
        assertEquals(2, mMessageHandler.getStopRequestsForTest().get(CLIENT_ID1).size(), 2);
        inOrder.verify(mSessionController).endSession();
    }

    @Test
    public void testAppMessageWithExistingNamespace() throws JSONException {
        setUpForAppMessageTest();

        JSONObject actualMessage = buildActualAppMessage();
        JSONObject message = buildAppMessage(CLIENT_ID1, NAMESPACE1, actualMessage);
        assertTrue(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler)
                .sendJsonCastMessage(argThat(new JSONObjectLike(actualMessage)), eq(NAMESPACE1),
                        eq(CLIENT_ID1), eq(SEQUENCE_NUMBER1));
    }

    @Test
    public void testAppMessageWithNonexistingNamespace() throws JSONException {
        setUpForAppMessageTest();

        JSONObject actualMessage = buildActualAppMessage();
        JSONObject message = buildAppMessage(CLIENT_ID1, NAMESPACE2, actualMessage);
        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler, never())
                .sendJsonCastMessage(any(JSONObject.class), anyString(), anyString(), anyInt());
    }

    @Test
    public void testAppMessageWithoutSequenceNumber() throws JSONException {
        setUpForAppMessageTest();

        JSONObject actualMessage = buildActualAppMessage();
        JSONObject message = buildAppMessage(CLIENT_ID1, NAMESPACE1, actualMessage);
        message.remove("sequenceNumber");
        assertTrue(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler)
                .sendJsonCastMessage(argThat(new JSONObjectLike(actualMessage)), eq(NAMESPACE1),
                        eq(CLIENT_ID1), eq(VOID_SEQUENCE_NUMBER));
    }

    @Test
    public void testAppMessageWithNullSessionId() throws JSONException {
        setUpForAppMessageTest();

        JSONObject actualMessage = buildActualAppMessage();
        final JSONObject message = buildAppMessage(CLIENT_ID1, NAMESPACE1, actualMessage);
        message.getJSONObject("message").remove("sessionId");
        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler, never())
                .sendJsonCastMessage(any(JSONObject.class), anyString(), anyString(), anyInt());
    }

    @Test
    public void testAppMessageWithWrongSessionId() throws JSONException {
        setUpForAppMessageTest();

        JSONObject actualMessage = buildActualAppMessage();
        JSONObject message = buildAppMessage(CLIENT_ID1, NAMESPACE1, actualMessage);
        message.getJSONObject("message").put("sessionId", INVALID_SESSION_ID);
        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler, never())
                .sendJsonCastMessage(any(JSONObject.class), anyString(), anyString(), anyInt());
    }

    @Test
    public void testAppMessageWithNullActualMessage() throws JSONException {
        setUpForAppMessageTest();

        JSONObject actualMessage = buildActualAppMessage();
        final JSONObject message = buildAppMessage(CLIENT_ID1, NAMESPACE1, actualMessage);
        message.getJSONObject("message").remove("message");
        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler, never())
                .sendJsonCastMessage(any(JSONObject.class), anyString(), anyString(), anyInt());
    }

    @Test
    public void testAppMessageWithStringMessage() throws JSONException {
        setUpForAppMessageTest();

        JSONObject actualMessage = buildActualAppMessage();
        JSONObject message = buildAppMessage(CLIENT_ID1, NAMESPACE1, actualMessage);
        message.getJSONObject("message").put("message", "string message");
        assertTrue(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler, never())
                .sendJsonCastMessage(any(JSONObject.class), anyString(), anyString(), anyInt());
        verify(mMessageHandler)
                .sendStringCastMessage(
                        eq("string message"), eq(NAMESPACE1), eq(CLIENT_ID1), eq(SEQUENCE_NUMBER1));
    }

    @Test
    public void testAppMessageWithNullAppMessage() throws JSONException {
        setUpForAppMessageTest();

        JSONObject actualMessage = buildActualAppMessage();
        final JSONObject message = buildAppMessage(CLIENT_ID1, NAMESPACE1, actualMessage);
        message.remove("message");
        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler, never())
                .sendJsonCastMessage(any(JSONObject.class), anyString(), anyString(), anyInt());
    }

    @Test
    public void testAppMessageWithEmptyAppMessage() throws JSONException {
        setUpForAppMessageTest();

        JSONObject actualMessage = buildActualAppMessage();
        final JSONObject message = buildAppMessage(CLIENT_ID1, NAMESPACE1, actualMessage);
        message.put("message", new JSONObject());
        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler, never())
                .sendJsonCastMessage(any(JSONObject.class), anyString(), anyString(), anyInt());
    }

    @Test
    public void testAppMessageWithWrongTypeAppMessage() throws JSONException {
        setUpForAppMessageTest();

        JSONObject actualMessage = buildActualAppMessage();
        final JSONObject message = buildAppMessage(CLIENT_ID1, NAMESPACE1, actualMessage);
        message.put("message", "wrong type app message");
        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler, never())
                .sendJsonCastMessage(any(JSONObject.class), anyString(), anyString(), anyInt());
    }

    @Test
    public void testAppMessageWithNullClient() throws JSONException {
        setUpForAppMessageTest();

        JSONObject actualMessage = buildActualAppMessage();
        final JSONObject message = buildAppMessage(CLIENT_ID1, NAMESPACE1, actualMessage);
        message.remove("clientId");
        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler, never())
                .sendJsonCastMessage(any(JSONObject.class), anyString(), anyString(), anyInt());
    }

    @Test
    public void testAppMessageWithNonexistingClient() throws JSONException {
        setUpForAppMessageTest();

        JSONObject actualMessage = buildActualAppMessage();
        JSONObject message = buildAppMessage(CLIENT_ID1, NAMESPACE1, actualMessage);
        message.put("clientId", INVALID_CLIENT_ID);
        assertFalse(mMessageHandler.handleMessageFromClient(message.toString()));
        verify(mMessageHandler, never())
                .sendJsonCastMessage(any(JSONObject.class), anyString(), anyString(), anyInt());
    }

    @Test
    public void testSendJsonCastMessage() throws JSONException {
        assertEquals(mMessageHandler.getRequestsForTest().size(), 0);
        JSONObject message = buildJsonCastMessage("message");
        assertTrue(mMessageHandler.sendJsonCastMessage(
                message, NAMESPACE1, CLIENT_ID1, SEQUENCE_NUMBER1));
        assertEquals(mMessageHandler.getRequestsForTest().size(), 1);
        verify(mMessageHandler)
                .sendStringCastMessage(
                        argThat(new JSONStringLike(message)), anyString(), anyString(), anyInt());
    }

    @Test
    public void testSendJsonCastMessageWhileDisconnected() throws JSONException {
        doReturn(false).when(mSessionController).isConnected();

        JSONObject message = buildJsonCastMessage("message");
        assertFalse(mMessageHandler.sendJsonCastMessage(
                message, NAMESPACE1, CLIENT_ID1, SEQUENCE_NUMBER1));
        assertEquals(mMessageHandler.getRequestsForTest().size(), 0);
        verify(mMessageHandler, never())
                .sendStringCastMessage(anyString(), anyString(), anyString(), anyInt());
    }

    @Test
    public void testSendJsonCastMessageWithInvalidSequenceNumber() throws JSONException {
        JSONObject message = buildJsonCastMessage("message");
        assertTrue(mMessageHandler.sendJsonCastMessage(
                message, NAMESPACE1, CLIENT_ID1, VOID_SEQUENCE_NUMBER));
        assertEquals(mMessageHandler.getRequestsForTest().size(), 0);
        verify(mMessageHandler)
                .sendStringCastMessage(
                        argThat(new JSONStringLike(message)), anyString(), anyString(), anyInt());
    }

    @Test
    public void testSendJsonCastMessageWithNullRequestId() throws JSONException {
        assertEquals(mMessageHandler.getRequestsForTest().size(), 0);
        JSONObject message = buildJsonCastMessage("message");
        message.remove("requestId");
        assertTrue(mMessageHandler.sendJsonCastMessage(
                message, NAMESPACE1, CLIENT_ID1, SEQUENCE_NUMBER1));
        assertTrue(message.has("requestId"));
        assertEquals(mMessageHandler.getRequestsForTest().size(), 1);
        verify(mMessageHandler)
                .sendStringCastMessage(
                        argThat(new JSONStringLike(message)), anyString(), anyString(), anyInt());
    }

    @Test
    public void testOnMessageReceivedWithExistingRequestId() throws JSONException {
        doNothing()
                .when(mMessageHandler)
                .onAppMessage(anyString(), anyString(), any(RequestRecord.class));
        assertEquals(mMessageHandler.getRequestsForTest().size(), 0);
        RequestRecord request = new RequestRecord(CLIENT_ID1, SEQUENCE_NUMBER1);
        mMessageHandler.getRequestsForTest().append(REQUEST_ID1, request);
        assertEquals(mMessageHandler.getRequestsForTest().size(), 1);
        JSONObject message = new JSONObject();
        message.put("requestId", REQUEST_ID1);
        mMessageHandler.onMessageReceived(NAMESPACE1, message.toString());
        assertEquals(mMessageHandler.getRequestsForTest().size(), 0);
        verify(mMessageHandler).onAppMessage(eq(message.toString()), eq(NAMESPACE1), eq(request));
    }

    @Test
    public void testOnMessageReceivedWithNonexistingRequestId() throws JSONException {
        doNothing()
                .when(mMessageHandler)
                .onAppMessage(anyString(), anyString(), any(RequestRecord.class));
        assertEquals(mMessageHandler.getRequestsForTest().size(), 0);
        RequestRecord request = new RequestRecord(CLIENT_ID1, SEQUENCE_NUMBER1);
        mMessageHandler.getRequestsForTest().append(REQUEST_ID1, request);
        assertEquals(mMessageHandler.getRequestsForTest().size(), 1);
        JSONObject message = new JSONObject();
        message.put("requestId", REQUEST_ID2);
        mMessageHandler.onMessageReceived(NAMESPACE1, message.toString());
        assertEquals(mMessageHandler.getRequestsForTest().size(), 1);
        verify(mMessageHandler)
                .onAppMessage(eq(message.toString()), eq(NAMESPACE1), (RequestRecord) isNull());
    }

    @Test
    public void testOnMessageReceivedWithoutRequestId() {
        doNothing()
                .when(mMessageHandler)
                .onAppMessage(anyString(), anyString(), any(RequestRecord.class));
        assertEquals(mMessageHandler.getRequestsForTest().size(), 0);
        RequestRecord request = new RequestRecord(CLIENT_ID1, SEQUENCE_NUMBER1);
        mMessageHandler.getRequestsForTest().append(REQUEST_ID1, request);
        assertEquals(mMessageHandler.getRequestsForTest().size(), 1);
        JSONObject message = new JSONObject();
        mMessageHandler.onMessageReceived(NAMESPACE1, message.toString());
        assertEquals(mMessageHandler.getRequestsForTest().size(), 1);
        verify(mMessageHandler)
                .onAppMessage(eq(message.toString()), eq(NAMESPACE1), (RequestRecord) isNull());
    }

    @Test
    public void testOnMessageReceivedOfMediaNamespace() {
        doNothing().when(mMessageHandler).onMediaMessage(anyString(), any(RequestRecord.class));
        mMessageHandler.onMessageReceived(MEDIA_NAMESPACE, "anymessage");
        verify(mMessageHandler).onMediaMessage(eq("anymessage"), (RequestRecord) isNull());
    }

    @Test
    public void testOnMediaMessageOfMediaStatusTypeWithRequestRecord() {
        doNothing()
                .when(mMessageHandler)
                .sendEnclosedMessageToClient(anyString(), anyString(), anyString(), anyInt());
        doReturn(true).when(mMessageHandler).isMediaStatusMessage(anyString());
        RequestRecord request = new RequestRecord(CLIENT_ID1, SEQUENCE_NUMBER1);
        mMessageHandler.onMediaMessage("anymessage", request);
        verify(mMessageHandler, never())
                .sendEnclosedMessageToClient(eq(CLIENT_ID1), eq("v2_message"), eq("anymessage"),
                        eq(VOID_SEQUENCE_NUMBER));
        verify(mMessageHandler)
                .sendEnclosedMessageToClient(eq(CLIENT_ID2), eq("v2_message"), eq("anymessage"),
                        eq(VOID_SEQUENCE_NUMBER));
        verify(mMessageHandler)
                .sendEnclosedMessageToClient(
                        eq(CLIENT_ID1), eq("v2_message"), eq("anymessage"), eq(SEQUENCE_NUMBER1));
    }

    @Test
    public void testOnMediaMessageOfMediaStatusTypeWithNullRequestRecord() {
        doNothing()
                .when(mMessageHandler)
                .sendEnclosedMessageToClient(anyString(), anyString(), anyString(), anyInt());
        doReturn(true).when(mMessageHandler).isMediaStatusMessage(anyString());
        mMessageHandler.onMediaMessage("anymessage", null);
        verify(mMessageHandler)
                .sendEnclosedMessageToClient(eq(CLIENT_ID1), eq("v2_message"), eq("anymessage"),
                        eq(VOID_SEQUENCE_NUMBER));
        verify(mMessageHandler)
                .sendEnclosedMessageToClient(eq(CLIENT_ID2), eq("v2_message"), eq("anymessage"),
                        eq(VOID_SEQUENCE_NUMBER));
    }

    @Test
    public void testOnMediaMessageOfNonMediaStatusTypeWithRequestRecord() {
        doNothing()
                .when(mMessageHandler)
                .sendEnclosedMessageToClient(anyString(), anyString(), anyString(), anyInt());
        doReturn(false).when(mMessageHandler).isMediaStatusMessage(anyString());
        RequestRecord request = new RequestRecord(CLIENT_ID1, SEQUENCE_NUMBER1);
        mMessageHandler.onMediaMessage("anymessage", request);
        verify(mMessageHandler, never())
                .sendEnclosedMessageToClient(
                        anyString(), anyString(), anyString(), eq(VOID_SEQUENCE_NUMBER));
        verify(mMessageHandler)
                .sendEnclosedMessageToClient(
                        eq(CLIENT_ID1), eq("v2_message"), eq("anymessage"), eq(SEQUENCE_NUMBER1));
    }

    @Test
    public void testOnMediaMessageOfNonMediaStatusTypeWithNullRequestRecord() {
        doNothing()
                .when(mMessageHandler)
                .sendEnclosedMessageToClient(anyString(), anyString(), anyString(), anyInt());
        doReturn(false).when(mMessageHandler).isMediaStatusMessage(anyString());
        mMessageHandler.onMediaMessage("anymessage", null);
        verify(mMessageHandler, never())
                .sendEnclosedMessageToClient(
                        anyString(), anyString(), anyString(), eq(VOID_SEQUENCE_NUMBER));
        verify(mMessageHandler, never())
                .sendEnclosedMessageToClient(anyString(), anyString(), anyString(), anyInt());
    }

    @Test
    public void testOnAppMessageWithRequestRecord() throws JSONException {
        doNothing()
                .when(mMessageHandler)
                .sendEnclosedMessageToClient(anyString(), anyString(), anyString(), anyInt());
        RequestRecord request = new RequestRecord(CLIENT_ID1, SEQUENCE_NUMBER1);
        mMessageHandler.onAppMessage("anyMessage", NAMESPACE1, request);
        JSONObject expected = new JSONObject();
        expected.put("sessionId", SESSION_ID);
        expected.put("namespaceName", NAMESPACE1);
        expected.put("message", "anyMessage");
        verify(mMessageHandler)
                .sendEnclosedMessageToClient(eq(CLIENT_ID1), eq("app_message"),
                        argThat(new JSONStringLike(expected)), eq(SEQUENCE_NUMBER1));
        verify(mMessageHandler, never()).broadcastClientMessage(anyString(), anyString());
    }

    @Test
    public void testOnAppMessageWithNullRequestRecord() throws JSONException {
        doNothing().when(mMessageHandler).broadcastClientMessage(anyString(), anyString());
        mMessageHandler.onAppMessage("anyMessage", NAMESPACE1, null);
        JSONObject expected = new JSONObject();
        expected.put("sessionId", SESSION_ID);
        expected.put("namespaceName", NAMESPACE1);
        expected.put("message", "anyMessage");
        verify(mMessageHandler, never())
                .sendEnclosedMessageToClient(anyString(), anyString(), anyString(), anyInt());
        verify(mMessageHandler)
                .broadcastClientMessage(eq("app_message"), argThat(new JSONStringLike(expected)));
    }

    @Test
    public void testOnSessionEnded() {
        doNothing()
                .when(mMessageHandler)
                .sendEnclosedMessageToClient(anyString(), anyString(), anyString(), anyInt());
        assertEquals(0, mMessageHandler.getStopRequestsForTest().size());
        mMessageHandler.getStopRequestsForTest().put(CLIENT_ID1, new ArrayDeque<Integer>());
        mMessageHandler.getStopRequestsForTest().get(CLIENT_ID1).add(SEQUENCE_NUMBER1);
        mMessageHandler.getStopRequestsForTest().get(CLIENT_ID1).add(SEQUENCE_NUMBER2);
        assertEquals(1, mMessageHandler.getStopRequestsForTest().size());
        assertEquals(2, mMessageHandler.getStopRequestsForTest().get(CLIENT_ID1).size());

        mMessageHandler.onSessionEnded();
        verify(mMessageHandler)
                .sendEnclosedMessageToClient(
                        eq(CLIENT_ID1), eq("remove_session"), eq(SESSION_ID), eq(SEQUENCE_NUMBER1));
        verify(mMessageHandler)
                .sendEnclosedMessageToClient(
                        eq(CLIENT_ID1), eq("remove_session"), eq(SESSION_ID), eq(SEQUENCE_NUMBER2));
        verify(mMessageHandler)
                .sendEnclosedMessageToClient(eq(CLIENT_ID2), eq("remove_session"), eq(SESSION_ID),
                        eq(VOID_SEQUENCE_NUMBER));
    }

    @Test
    public void testOnVolumeChanged() {
        doNothing().when(mMessageHandler).onVolumeChanged(anyString(), anyInt());
        assertEquals(0, mMessageHandler.getVolumeRequestsForTest().size());
        mMessageHandler.getVolumeRequestsForTest().add(
                new RequestRecord(CLIENT_ID1, SEQUENCE_NUMBER1));
        assertEquals(1, mMessageHandler.getVolumeRequestsForTest().size());

        mMessageHandler.onVolumeChanged();
        verify(mMessageHandler).onVolumeChanged(CLIENT_ID1, SEQUENCE_NUMBER1);
        assertEquals(0, mMessageHandler.getVolumeRequestsForTest().size());
    }

    @Test
    public void testOnVolumeChangedWithEmptyVolumeRequests() {
        mMessageHandler.onVolumeChanged();
        verify(mMessageHandler, never()).onVolumeChanged(eq(CLIENT_ID1), eq(SEQUENCE_NUMBER1));
    }

    @Test
    public void testOnVolumeChangedForClient() {
        doNothing()
                .when(mMessageHandler)
                .sendEnclosedMessageToClient(anyString(), anyString(), anyString(), anyInt());
        mMessageHandler.onVolumeChanged(CLIENT_ID1, SEQUENCE_NUMBER1);
        verify(mMessageHandler)
                .sendEnclosedMessageToClient(
                        eq(CLIENT_ID1), eq("v2_message"), (String) isNull(), eq(SEQUENCE_NUMBER1));
    }

    @Test
    public void testOnAppMessageSent() {
        Status mockResult = mock(Status.class);
        doReturn(true).when(mockResult).isSuccess();
        doNothing()
                .when(mMessageHandler)
                .sendEnclosedMessageToClient(anyString(), anyString(), anyString(), anyInt());
        mMessageHandler.onSendAppMessageResult(mockResult, CLIENT_ID1, SEQUENCE_NUMBER1);
        verify(mMessageHandler)
                .sendEnclosedMessageToClient(
                        eq(CLIENT_ID1), eq("app_message"), (String) isNull(), eq(SEQUENCE_NUMBER1));
    }

    @Test
    public void testSendEnclosedMessageToClient() {
        String message = "dontcare_message1";
        doReturn(message)
                .when(mMessageHandler)
                .buildEnclosedClientMessage(anyString(), anyString(), anyString(), anyInt());
        mMessageHandler.sendEnclosedMessageToClient(
                CLIENT_ID1, "anytype", "dontcare_message2", SEQUENCE_NUMBER1);

        verify(mRouteProvider).sendMessageToClient(eq(CLIENT_ID1), eq(message));
    }

    @Test
    public void testBroadcastClientMessage() {
        doNothing()
                .when(mMessageHandler)
                .sendEnclosedMessageToClient(anyString(), anyString(), anyString(), anyInt());
        mMessageHandler.broadcastClientMessage("anytype", "anymessage");
        for (String clientId : mRouteProvider.getClientIdToRecords().keySet()) {
            verify(mMessageHandler)
                    .sendEnclosedMessageToClient(eq(clientId), eq("anytype"), eq("anymessage"),
                            eq(VOID_SEQUENCE_NUMBER));
        }
    }

    @Test
    public void testSendReceiverActionToClient() throws JSONException {
        MediaSink sink = mock(MediaSink.class);
        doReturn("sink-id").when(sink).getId();
        doReturn("sink-name").when(sink).getName();
        doReturn(Arrays.asList("audio_in", "audio_out", "video_in", "video_out"))
                .when(mSessionController)
                .getCapabilities();

        mMessageHandler.sendReceiverActionToClient("route-id", sink, "client-id", "action");

        ArgumentCaptor<String> messageCaptor = ArgumentCaptor.forClass(String.class);
        verify(mRouteProvider).sendMessageToClient(eq("client-id"), messageCaptor.capture());
        JSONObject capturedMessage = new JSONObject(messageCaptor.getValue());
        assertEquals(capturedMessage.getString("type"), "receiver_action");
        assertEquals(capturedMessage.getInt("sequenceNumber"), VOID_SEQUENCE_NUMBER);
        assertEquals(capturedMessage.getInt("timeoutMillis"), 0);
        assertEquals(capturedMessage.getString("clientId"), "client-id");

        JSONObject receiverAction = capturedMessage.getJSONObject("message");
        assertEquals(receiverAction.getString("action"), "action");

        JSONObject receiverInfo = receiverAction.getJSONObject("receiver");
        assertEquals(receiverInfo.getString("label"), "sink-id");
        assertEquals(receiverInfo.getString("friendlyName"), "sink-name");
        assertFalse(receiverInfo.has("volume"));
        assertFalse(receiverInfo.has("isActiveInput"));
        assertFalse(receiverInfo.has("displayStatus"));
        assertEquals(receiverInfo.getString("receiverType"), "cast");

        JSONArray capabilities = receiverInfo.getJSONArray("capabilities");
        assertEquals(capabilities.getString(0), "audio_in");
        assertEquals(capabilities.getString(1), "audio_out");
        assertEquals(capabilities.getString(2), "video_in");
        assertEquals(capabilities.getString(3), "video_out");
    }

    @Test
    public void testBuildEnclosedClientMessageWithNullMessage() throws JSONException {
        String message = mMessageHandler.buildEnclosedClientMessage(
                "anytype", null, CLIENT_ID1, SEQUENCE_NUMBER1);
        JSONObject expected = new JSONObject();
        expected.put("type", "anytype");
        expected.put("sequenceNumber", SEQUENCE_NUMBER1);
        expected.put("timeoutMillis", 0);
        expected.put("clientId", CLIENT_ID1);
        expected.put("message", null);

        assertTrue("\nexpected: " + expected.toString() + ",\n  actual: " + message.toString(),
                new JSONObjectLike(expected).matches(new JSONObject(message)));
    }

    @Test
    public void testBuildEnclosedClientMessageOfRemoveSessionType() throws JSONException {
        String message = mMessageHandler.buildEnclosedClientMessage(
                "remove_session", SESSION_ID, CLIENT_ID1, SEQUENCE_NUMBER1);
        JSONObject expected = new JSONObject();
        expected.put("type", "remove_session");
        expected.put("sequenceNumber", SEQUENCE_NUMBER1);
        expected.put("timeoutMillis", 0);
        expected.put("clientId", CLIENT_ID1);
        expected.put("message", SESSION_ID);

        assertTrue("\nexpected: " + expected.toString() + ",\n  actual: " + message.toString(),
                new JSONObjectLike(expected).matches(new JSONObject(message)));
    }

    @Test
    public void testBuildEnclosedClientMessageOfDisconnectSessionType() throws JSONException {
        String message = mMessageHandler.buildEnclosedClientMessage(
                "disconnect_session", SESSION_ID, CLIENT_ID1, SEQUENCE_NUMBER1);
        JSONObject expected = new JSONObject();
        expected.put("type", "disconnect_session");
        expected.put("sequenceNumber", SEQUENCE_NUMBER1);
        expected.put("timeoutMillis", 0);
        expected.put("clientId", CLIENT_ID1);
        expected.put("message", SESSION_ID);

        assertTrue("\nexpected: " + expected.toString() + ",\n  actual: " + message.toString(),
                new JSONObjectLike(expected).matches(new JSONObject(message)));
    }

    @Test
    public void testBuildEnclosedClientMessageWithInnerMessage() throws JSONException {
        JSONObject innerMessage = buildSessionMessage(SESSION_ID);
        String message = mMessageHandler.buildEnclosedClientMessage(
                "anytype", innerMessage.toString(), CLIENT_ID1, SEQUENCE_NUMBER1);
        JSONObject expected = new JSONObject();
        expected.put("type", "anytype");
        expected.put("sequenceNumber", SEQUENCE_NUMBER1);
        expected.put("timeoutMillis", 0);
        expected.put("clientId", CLIENT_ID1);
        expected.put("message", innerMessage);

        assertTrue("\nexpected: " + expected.toString() + ",\n  actual: " + message.toString(),
                new JSONObjectLike(expected).matches(new JSONObject(message)));
    }

    @Test
    public void testBuildSessionMessage() throws JSONException {
        CastDevice castDevice = mock(CastDevice.class);
        doReturn("device-id").when(castDevice).getDeviceId();
        doReturn("CastDevice friendly name").when(castDevice).getFriendlyName();
        doReturn(castDevice).when(mSession).getCastDevice();
        ApplicationMetadata appMetadata = mock(ApplicationMetadata.class);
        doReturn("app-id").when(appMetadata).getApplicationId();
        doReturn(appMetadata).when(mSession).getApplicationMetadata();
        doReturn(1.0).when(mSession).getVolume();
        doReturn(false).when(mSession).isMute();
        doReturn("status text").when(mSession).getApplicationStatus();
        doReturn(1).when(mSession).getActiveInputState();
        doReturn(Arrays.asList("namespace-1", "namespace-2"))
                .when(mSessionController)
                .getNamespaces();
        doReturn(Arrays.asList("audio_in", "audio_out", "video_in", "video_out"))
                .when(mSessionController)
                .getCapabilities();

        JSONObject message = new JSONObject(mMessageHandler.buildSessionMessage());

        assertEquals(message.getString("sessionId"), SESSION_ID);
        assertEquals(message.getString("statusText"), "status text");
        assertEquals(message.getString("status"), "connected");
        assertEquals(message.getString("transportId"), "web-4");
        assertEquals(message.getString("appId"), "app-id");
        assertEquals(message.getString("displayName"), "CastDevice friendly name");

        JSONArray mediaArray = message.getJSONArray("media");

        assertEquals(mediaArray.length(), 0);

        JSONArray namespacesArray = message.getJSONArray("namespaces");
        assertEquals(namespacesArray.length(), 2);
        assertEquals(namespacesArray.getJSONObject(0).getString("name"), "namespace-1");
        assertEquals(namespacesArray.getJSONObject(1).getString("name"), "namespace-2");

        JSONObject receiverInfo = message.getJSONObject("receiver");
        assertEquals(receiverInfo.getString("label"), "device-id");
        assertEquals(receiverInfo.getString("friendlyName"), "CastDevice friendly name");
        assertEquals(receiverInfo.getInt("isActiveInput"), 1);
        assertEquals(receiverInfo.getString("receiverType"), "cast");
        assertFalse(receiverInfo.has("displayStatus"));

        JSONArray capabilities = receiverInfo.getJSONArray("capabilities");
        assertEquals(capabilities.getString(0), "audio_in");
        assertEquals(capabilities.getString(1), "audio_out");
        assertEquals(capabilities.getString(2), "video_in");
        assertEquals(capabilities.getString(3), "video_out");

        JSONObject volume = receiverInfo.getJSONObject("volume");
        assertEquals(volume.getDouble("level"), 1.0, 1e-6);
        assertFalse(volume.getBoolean("muted"));
    }

    @Test
    public void testBuildSessionMessage_sessionDisconnected() {
        doReturn(false).when(mSessionController).isConnected();

        CastDevice castDevice = mock(CastDevice.class);
        doReturn("device-id").when(castDevice).getDeviceId();
        doReturn("CastDevice friendly name").when(castDevice).getFriendlyName();
        doReturn(castDevice).when(mSession).getCastDevice();
        ApplicationMetadata appMetadata = mock(ApplicationMetadata.class);
        doReturn("app-id").when(appMetadata).getApplicationId();
        doReturn(appMetadata).when(mSession).getApplicationMetadata();
        doReturn(1.0).when(mSession).getVolume();
        doReturn(false).when(mSession).isMute();
        doReturn("status text").when(mSession).getApplicationStatus();
        doReturn(1).when(mSession).getActiveInputState();
        doReturn(Arrays.asList("namespace-1", "namespace-2"))
                .when(mSessionController)
                .getNamespaces();
        doReturn(Arrays.asList("audio_in", "audio_out", "video_in", "video_out"))
                .when(mSessionController)
                .getCapabilities();

        assertEquals(mMessageHandler.buildSessionMessage(), "{}");
    }

    private JSONObject buildCastV2Message(String clientId, JSONObject innerMessage)
            throws JSONException {
        JSONObject message = new JSONObject();
        message.put("type", "v2_message");
        message.put("message", innerMessage);
        message.put("sequenceNumber", SEQUENCE_NUMBER1);
        message.put("timeoutMillis", 0);
        message.put("clientId", clientId);

        return message;
    }

    private JSONObject buildAppMessage(String clientId, String namespace, Object actualMessage)
            throws JSONException {
        JSONObject innerMessage = new JSONObject();
        innerMessage.put("sessionId", mSessionController.getSessionId());
        innerMessage.put("namespaceName", namespace);
        innerMessage.put("message", actualMessage);

        JSONObject message = new JSONObject();
        message.put("type", "app_message");
        message.put("message", innerMessage);
        message.put("sequenceNumber", SEQUENCE_NUMBER1);
        message.put("timeoutMillis", 0);
        message.put("clientId", clientId);

        return message;
    }

    private JSONObject buildActualAppMessage() throws JSONException {
        JSONObject message = new JSONObject();
        message.put("type", "actual app message type");

        return message;
    }

    private JSONObject buildSessionMessage(String sessionId) throws JSONException {
        JSONObject message = new JSONObject();
        message.put("sessionId", sessionId);

        return message;
    }

    private void expectException(CheckedRunnable r, Class exceptionClass) {
        boolean caughtException = false;
        try {
            r.run();
        } catch (Exception e) {
            if (e.getClass() == exceptionClass) caughtException = true;
        }
        assertTrue(caughtException);
    }

    private JSONObject buildJsonCastMessage(String message) throws JSONException {
        JSONObject jsonMessage = new JSONObject();
        jsonMessage.put("requestId", REQUEST_ID1);
        jsonMessage.put("message", message);
        return jsonMessage;
    }

    private void verifySimpleSessionMessage(JSONObject message, String type, int sequenceNumber,
            String clientId) throws JSONException {
        assertEquals(message.getString("type"), type);
        assertEquals(message.getInt("sequenceNumber"), sequenceNumber);
        assertEquals(message.getInt("timeoutMillis"), 0);
        assertEquals(message.getInt("sequenceNumber"), sequenceNumber);
    }

    private void prepareClientRecord(String routeId, String clientId, String appId,
            String autoJoinPolicy, String origin, int tabId) {
        ClientRecord clientRecord =
                new ClientRecord(routeId, clientId, appId, autoJoinPolicy, origin, tabId);
        mClientRecordMap.put(clientId, clientRecord);
    }

    private JSONObject buildLeaveSessionMessage(
            String clientId, String sessionId, int sequenceNumber) throws JSONException {
        JSONObject message = new JSONObject();
        message.put("type", "leave_session");
        message.put("clientId", clientId);
        message.put("message", sessionId);
        message.put("sequenceNumber", sequenceNumber);
        return message;
    }
}
