// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.router.caf;

import static org.chromium.chrome.browser.media.router.caf.CastUtils.isSameOrigin;

import android.os.Handler;
import android.support.v4.util.ArrayMap;
import android.text.TextUtils;
import android.util.SparseArray;

import androidx.annotation.VisibleForTesting;

import com.google.android.gms.cast.ApplicationMetadata;
import com.google.android.gms.common.api.PendingResult;
import com.google.android.gms.common.api.Status;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.chrome.browser.media.router.CastRequestIdGenerator;
import org.chromium.chrome.browser.media.router.CastSessionUtil;
import org.chromium.chrome.browser.media.router.ClientRecord;
import org.chromium.chrome.browser.media.router.MediaSink;

import java.io.IOException;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Queue;

/**
 * The handler for cast messages. It receives events between the Cast SDK and the page, process and
 * dispatch the messages accordingly. The handler talks to the Cast SDK via CastSession, and
 * talks to the pages via the media router.
 */
public class CafMessageHandler {
    private static final String TAG = "CafMR";

    // Sequence number used when no sequence number is required or was initially passed.
    static final int VOID_SEQUENCE_NUMBER = -1;
    static final int TIMEOUT_IMMEDIATE = 0;

    private static final String MEDIA_MESSAGE_TYPES[] = {
            "PLAY",
            "LOAD",
            "PAUSE",
            "SEEK",
            "STOP_MEDIA",
            "MEDIA_SET_VOLUME",
            "MEDIA_GET_STATUS",
            "EDIT_TRACKS_INFO",
            "QUEUE_LOAD",
            "QUEUE_INSERT",
            "QUEUE_UPDATE",
            "QUEUE_REMOVE",
            "QUEUE_REORDER",
    };

    private static final String MEDIA_SUPPORTED_COMMANDS[] = {
            "pause",
            "seek",
            "stream_volume",
            "stream_mute",
    };

    // Lock used to lazy initialize sMediaOverloadedMessageTypes.
    private static final Object INIT_LOCK = new Object();

    // Map associating types that have a different names outside of the media namespace and inside.
    // In other words, some types are sent as MEDIA_FOO or FOO_MEDIA by the client by the Cast
    // expect them to be named FOO. The reason being that FOO might exist in multiple namespaces
    // but the client isn't aware of namespacing.
    private static Map<String, String> sMediaOverloadedMessageTypes;

    private SparseArray<RequestRecord> mRequests;
    private ArrayMap<String, Queue<Integer>> mStopRequests;
    private Queue<RequestRecord> mVolumeRequests;

    private final CastSessionController mSessionController;
    private final CafMediaRouteProvider mRouteProvider;
    private Handler mHandler;

    /**
     * The record for client requests. {@link CafMessageHandler} uses this class to manage the
     * client requests and match responses to the requests.
     */
    static class RequestRecord {
        public final String clientId;
        public final int sequenceNumber;

        public RequestRecord(String clientId, int sequenceNumber) {
            this.clientId = clientId;
            this.sequenceNumber = sequenceNumber;
        }
    }

    /**
     * Initializes a new {@link CafMessageHandler} instance.
     * @param session  The {@link CastSession} for communicating with the Cast SDK.
     * @param provider The {@link CafMediaRouteProvider} for communicating with the page.
     */
    public CafMessageHandler(
            CafMediaRouteProvider provider, CastSessionController sessionController) {
        mRouteProvider = provider;
        mRequests = new SparseArray<RequestRecord>();
        mStopRequests = new ArrayMap<String, Queue<Integer>>();
        mSessionController = sessionController;
        mVolumeRequests = new ArrayDeque<RequestRecord>();
        mHandler = new Handler();

        synchronized (INIT_LOCK) {
            if (sMediaOverloadedMessageTypes == null) {
                sMediaOverloadedMessageTypes = new HashMap<String, String>();
                sMediaOverloadedMessageTypes.put("STOP_MEDIA", "STOP");
                sMediaOverloadedMessageTypes.put("MEDIA_SET_VOLUME", "SET_VOLUME");
                sMediaOverloadedMessageTypes.put("MEDIA_GET_STATUS", "GET_STATUS");
            }
        }
    }

    @VisibleForTesting
    static String[] getMediaMessageTypesForTest() {
        return MEDIA_MESSAGE_TYPES;
    }

    @VisibleForTesting
    static Map<String, String> getMediaOverloadedMessageTypesForTest() {
        return sMediaOverloadedMessageTypes;
    }

    @VisibleForTesting
    SparseArray<RequestRecord> getRequestsForTest() {
        return mRequests;
    }

    @VisibleForTesting
    Queue<RequestRecord> getVolumeRequestsForTest() {
        return mVolumeRequests;
    }

    @VisibleForTesting
    Map<String, Queue<Integer>> getStopRequestsForTest() {
        return mStopRequests;
    }

    /**
     * Set the session when a session is started, and notify all clients that are not connected.
     * @param session The newly created session.
     */
    public void onSessionStarted() {
        for (ClientRecord client : mRouteProvider.getClientIdToRecords().values()) {
            if (!client.isConnected) continue;

            notifySessionConnectedToClient(client.clientId);
        }
    }

    /** Notify a client that a session has connected. */
    private void notifySessionConnectedToClient(String clientId) {
        sendEnclosedMessageToClient(
                clientId, "new_session", buildSessionMessage(), VOID_SEQUENCE_NUMBER);
    }

    /**
     * Handle a message sent from client.
     *
     * @return whether the message has been handled successfully
     */
    public boolean handleMessageFromClient(String message) {
        try {
            JSONObject jsonMessage = new JSONObject(message);

            String messageType = jsonMessage.optString("type");
            switch (messageType) {
                case "client_connect":
                    return handleClientConnectMessage(jsonMessage);
                case "client_disconnect":
                    return handleClientDisconnectMessage(jsonMessage);
                case "leave_session":
                    return handleClientLeaveSessionMessage(jsonMessage);
                default: // fall out
            }
            return handleSessionMessageFromClient(jsonMessage);
        } catch (JSONException e) {
            Log.e(TAG, "JSONException while handling internal message: " + e);
        }
        return false;
    }

    private boolean handleClientConnectMessage(JSONObject jsonMessage) throws JSONException {
        String clientId = jsonMessage.getString("clientId");
        if (clientId == null) return false;

        ClientRecord clientRecord = mRouteProvider.getClientIdToRecords().get(clientId);
        if (clientRecord == null) return false;

        clientRecord.isConnected = true;
        if (mSessionController.isConnected()) {
            notifySessionConnectedToClient(clientRecord.clientId);
        }
        mRouteProvider.flushPendingMessagesToClient(clientRecord);

        return true;
    }

    private boolean handleClientDisconnectMessage(JSONObject jsonMessage) throws JSONException {
        String clientId = jsonMessage.getString("clientId");
        if (clientId == null) return false;

        ClientRecord client = mRouteProvider.getClientIdToRecords().get(clientId);
        if (client == null) return false;

        mRouteProvider.removeRoute(client.routeId, /* error= */ null);

        return true;
    }

    private boolean handleClientLeaveSessionMessage(JSONObject jsonMessage) throws JSONException {
        String clientId = jsonMessage.getString("clientId");
        if (clientId == null || !mSessionController.isConnected()) return false;

        String sessionId = jsonMessage.getString("message");
        if (!mSessionController.getSessionId().equals(sessionId)) return false;

        ClientRecord currentClient = mRouteProvider.getClientIdToRecords().get(clientId);
        if (currentClient == null) return false;

        int sequenceNumber = jsonMessage.optInt("sequenceNumber", VOID_SEQUENCE_NUMBER);

        // The web sender SDK doesn't actually recognize "leave_session" response, but this is to
        // acknowledge the "leave_session" request.
        mRouteProvider.sendMessageToClient(
                clientId, buildSimpleSessionMessage("leave_session", sequenceNumber, clientId));

        List<ClientRecord> leavingClients = new ArrayList<>();

        for (ClientRecord client : mRouteProvider.getClientIdToRecords().values()) {
            boolean shouldNotifyClient = false;
            if (CastMediaSource.AUTOJOIN_TAB_AND_ORIGIN_SCOPED.equals(currentClient.autoJoinPolicy)
                    && isSameOrigin(client.origin, currentClient.origin)
                    && client.tabId == currentClient.tabId) {
                shouldNotifyClient = true;
            } else if (CastMediaSource.AUTOJOIN_ORIGIN_SCOPED.equals(currentClient.autoJoinPolicy)
                    && isSameOrigin(client.origin, currentClient.origin)) {
                shouldNotifyClient = true;
            }
            if (shouldNotifyClient) {
                leavingClients.add(client);
            }
        }

        for (ClientRecord client : leavingClients) {
            mRouteProvider.removeRoute(client.routeId, /* error= */ null);
        }

        return true;
    }

    /** Builds a simple message for session-related events. */
    private String buildSimpleSessionMessage(String type, int sequenceNumber, String clientId)
            throws JSONException {
        JSONObject jsonMessage = new JSONObject();
        jsonMessage.put("type", type);
        jsonMessage.put("sequenceNumber", sequenceNumber);
        jsonMessage.put("timeoutMillis", TIMEOUT_IMMEDIATE);
        jsonMessage.put("clientId", clientId);
        return jsonMessage.toString();
    }

    /////////////////////////////////////////////////////////////////////////////////////////////
    // Functions for handling messages from the page to the Cast device.

    /**
     * Handles messages related to the cast session, i.e. messages happening on a established
     * connection. All these messages are sent from the page to the Cast SDK.
     * @param message The JSONObject message to be handled.
     */
    private boolean handleSessionMessageFromClient(JSONObject message) throws JSONException {
        String messageType = message.getString("type");
        if ("v2_message".equals(messageType)) {
            return handleCastV2MessageFromClient(message);
        } else if ("app_message".equals(messageType)) {
            return handleAppMessageFromClient(message);
        } else {
            Log.e(TAG, "Unsupported message: %s", message);
            return false;
        }
    }

    // An example of the Cast V2 message:
    //    {
    //        "type": "v2_message",
    //        "message": {
    //          "type": "...",
    //          ...
    //        },
    //        "sequenceNumber": 0,
    //        "timeoutMillis": 0,
    //        "clientId": "144042901280235697"
    //    }
    @VisibleForTesting
    boolean handleCastV2MessageFromClient(JSONObject jsonMessage) throws JSONException {
        assert "v2_message".equals(jsonMessage.getString("type"));

        final String clientId = jsonMessage.getString("clientId");
        if (clientId == null || !mRouteProvider.getClientIdToRecords().containsKey(clientId)) {
            return false;
        }

        JSONObject jsonCastMessage = jsonMessage.getJSONObject("message");
        String messageType = jsonCastMessage.getString("type");
        final int sequenceNumber = jsonMessage.optInt("sequenceNumber", VOID_SEQUENCE_NUMBER);

        if ("STOP".equals(messageType)) {
            handleStopMessage(clientId, sequenceNumber);
            return true;
        }

        if ("SET_VOLUME".equals(messageType)) {
            return handleVolumeMessage(
                    jsonCastMessage.getJSONObject("volume"), clientId, sequenceNumber);
        }

        if (Arrays.asList(MEDIA_MESSAGE_TYPES).contains(messageType)) {
            if (sMediaOverloadedMessageTypes.containsKey(messageType)) {
                messageType = sMediaOverloadedMessageTypes.get(messageType);
                jsonCastMessage.put("type", messageType);
            }
            return sendJsonCastMessage(
                    jsonCastMessage, CastSessionUtil.MEDIA_NAMESPACE, clientId, sequenceNumber);
        }

        return true;
    }

    boolean handleVolumeMessage(JSONObject volumeMessage, final String clientId,
            final int sequenceNumber) throws JSONException {
        if (volumeMessage == null) return false;
        if (!mSessionController.isConnected()) return false;
        boolean shouldWaitForVolumeChange = false;
        try {
            if (!volumeMessage.isNull("muted")) {
                boolean newMuted = volumeMessage.getBoolean("muted");
                if (mSessionController.getSession().isMute() != newMuted) {
                    mSessionController.getSession().setMute(newMuted);
                    shouldWaitForVolumeChange = true;
                }
            }
            if (!volumeMessage.isNull("level")) {
                double newLevel = volumeMessage.getDouble("level");
                double currentLevel = mSessionController.getSession().getVolume();
                if (!Double.isNaN(currentLevel)
                        && Math.abs(currentLevel - newLevel)
                                > CastSessionUtil.MIN_VOLUME_LEVEL_DELTA) {
                    mSessionController.getSession().setVolume(newLevel);
                    shouldWaitForVolumeChange = true;
                }
            }
        } catch (IOException | IllegalStateException e) {
            Log.e(TAG, "Failed to send volume command: " + e);
            return false;
        }

        // For each successful volume message we need to respond with an empty "v2_message" so the
        // Cast Web SDK can call the success callback of the page. If we expect the volume to change
        // as the result of the command, we're relying on {@link Cast.CastListener#onVolumeChanged}
        // to get called by the Android Cast SDK when the receiver status is updated. We keep the
        // sequence number until then. If the volume doesn't change as the result of the command, we
        // won't get notified by the Android SDK
        if (shouldWaitForVolumeChange) {
            mVolumeRequests.add(new RequestRecord(clientId, sequenceNumber));
        } else {
            // It's usually bad to have request and response on the same call stack so post the
            // response to the Android message loop.
            mHandler.post(new Runnable() {
                @Override
                public void run() {
                    onVolumeChanged(clientId, sequenceNumber);
                }
            });
        }
        return true;
    }

    @VisibleForTesting
    void handleStopMessage(String clientId, int sequenceNumber) {
        Queue<Integer> sequenceNumbersForClient = mStopRequests.get(clientId);
        if (sequenceNumbersForClient == null) {
            sequenceNumbersForClient = new ArrayDeque<Integer>();
            mStopRequests.put(clientId, sequenceNumbersForClient);
        }
        sequenceNumbersForClient.add(sequenceNumber);

        mSessionController.endSession();
    }

    // An example of the Cast application message:
    // {
    //   "type":"app_message",
    //   "message": {
    //     "sessionId":"...",
    //     "namespaceName":"...",
    //     "message": ...
    //   },
    //   "sequenceNumber":0,
    //   "timeoutMillis":3000,
    //   "clientId":"14417311915272175"
    // }
    @VisibleForTesting
    boolean handleAppMessageFromClient(JSONObject jsonMessage) throws JSONException {
        assert "app_message".equals(jsonMessage.getString("type"));

        String clientId = jsonMessage.getString("clientId");
        if (clientId == null || !mRouteProvider.getClientIdToRecords().containsKey(clientId)) {
            return false;
        }

        JSONObject jsonAppMessageWrapper = jsonMessage.getJSONObject("message");

        if (!mSessionController.getSessionId().equals(
                    jsonAppMessageWrapper.getString("sessionId"))) {
            return false;
        }

        String namespaceName = jsonAppMessageWrapper.getString("namespaceName");
        if (namespaceName == null || namespaceName.isEmpty()) return false;

        if (!mSessionController.getNamespaces().contains(namespaceName)) return false;

        int sequenceNumber = jsonMessage.optInt("sequenceNumber", VOID_SEQUENCE_NUMBER);

        Object actualMessageObject = jsonAppMessageWrapper.get("message");
        if (actualMessageObject == null) return false;

        if (actualMessageObject instanceof String) {
            String actualMessage = jsonAppMessageWrapper.getString("message");
            return sendStringCastMessage(actualMessage, namespaceName, clientId, sequenceNumber);
        }

        JSONObject actualMessage = jsonAppMessageWrapper.getJSONObject("message");
        return sendJsonCastMessage(actualMessage, namespaceName, clientId, sequenceNumber);
    }

    @VisibleForTesting
    boolean sendJsonCastMessage(JSONObject message, final String namespace, final String clientId,
            final int sequenceNumber) throws JSONException {
        if (!mSessionController.isConnected()) return false;

        removeNullFields(message);

        // Map the request id to a valid sequence number only.
        if (sequenceNumber != VOID_SEQUENCE_NUMBER) {
            // If for some reason, there is already a requestId other than 0, it
            // is kept. Otherwise, one is generated. In all cases it's associated with the
            // sequenceNumber passed by the client.
            int requestId = message.optInt("requestId", 0);
            if (requestId == 0) {
                requestId = CastRequestIdGenerator.getNextRequestId();
                message.put("requestId", requestId);
            }
            mRequests.append(requestId, new RequestRecord(clientId, sequenceNumber));
        }

        return sendStringCastMessage(message.toString(), namespace, clientId, sequenceNumber);
    }

    /////////////////////////////////////////////////////////////////////////////////////////////
    // Functions for handling messages from the Cast device to the pages.

    /**
     * Forwards the messages from the Cast device to the clients, and perform proper actions if it
     * is media message.
     * @param namespace The application specific namespace this message belongs to.
     * @param message The message within the namespace that's being sent by the receiver
     */
    public void onMessageReceived(String namespace, String message) {
        RequestRecord request = null;
        try {
            JSONObject jsonMessage = new JSONObject(message);
            int requestId = jsonMessage.getInt("requestId");
            if (mRequests.indexOfKey(requestId) >= 0) {
                request = mRequests.get(requestId);
                mRequests.delete(requestId);
            }
        } catch (JSONException e) {
        }

        if (CastSessionUtil.MEDIA_NAMESPACE.equals(namespace)) {
            onMediaMessage(message, request);
            return;
        }

        onAppMessage(message, namespace, request);
    }

    /**
     * Forwards the media message to the page via the media router.
     * The MEDIA_STATUS message needs to be sent to all the clients.
     * @param message The media that's being send by the receiver.
     * @param request The information about the client and the sequence number to respond with.
     */
    @VisibleForTesting
    void onMediaMessage(String message, RequestRecord request) {
        if (isMediaStatusMessage(message)) {
            // MEDIA_STATUS needs to be sent to all the clients.
            for (String clientId : mRouteProvider.getClientIdToRecords().keySet()) {
                if (request != null && clientId.equals(request.clientId)) continue;

                sendEnclosedMessageToClient(clientId, "v2_message", message, VOID_SEQUENCE_NUMBER);
            }
        }
        if (request != null) {
            sendEnclosedMessageToClient(
                    request.clientId, "v2_message", message, request.sequenceNumber);
        }
    }

    /**
     * Forwards the application specific message to the page via the media router.
     * @param message The message within the namespace that's being sent by the receiver.
     * @param namespace The application specific namespace this message belongs to.
     * @param request The information about the client and the sequence number to respond with.
     */
    @VisibleForTesting
    void onAppMessage(String message, String namespace, RequestRecord request) {
        try {
            JSONObject jsonMessage = new JSONObject();
            jsonMessage.put("sessionId", mSessionController.getSessionId());
            jsonMessage.put("namespaceName", namespace);
            jsonMessage.put("message", message);
            if (request != null) {
                sendEnclosedMessageToClient(request.clientId, "app_message", jsonMessage.toString(),
                        request.sequenceNumber);
            } else {
                broadcastClientMessage("app_message", jsonMessage.toString());
            }
        } catch (JSONException e) {
            Log.e(TAG, "Failed to create the message wrapper", e);
        }
    }

    /**
     * Notifies the session has stopped to all requesting clients.
     */
    public void onSessionEnded() {
        for (String clientId : mRouteProvider.getClientIdToRecords().keySet()) {
            Queue<Integer> sequenceNumbersForClient = mStopRequests.get(clientId);
            if (sequenceNumbersForClient == null) {
                sendEnclosedMessageToClient(clientId, "remove_session",
                        mSessionController.getSessionId(), VOID_SEQUENCE_NUMBER);
                continue;
            }

            for (int sequenceNumber : sequenceNumbersForClient) {
                sendEnclosedMessageToClient(clientId, "remove_session",
                        mSessionController.getSessionId(), sequenceNumber);
            }
            mStopRequests.remove(clientId);
        }
    }

    /**
     * When the Cast device volume really changed, updates the session status and notify all
     * requesting clients.
     */
    public void onVolumeChanged() {
        if (mVolumeRequests.isEmpty()) return;

        for (RequestRecord r : mVolumeRequests) onVolumeChanged(r.clientId, r.sequenceNumber);
        mVolumeRequests.clear();
    }

    @VisibleForTesting
    void onVolumeChanged(String clientId, int sequenceNumber) {
        sendEnclosedMessageToClient(clientId, "v2_message", null, sequenceNumber);
    }

    /**
     * Broadcasts the message to all clients.
     * @param type    The type of the message.
     * @param message The message to broadcast.
     */
    public void broadcastClientMessage(String type, String message) {
        for (String clientId : mRouteProvider.getClientIdToRecords().keySet()) {
            sendEnclosedMessageToClient(clientId, type, message, VOID_SEQUENCE_NUMBER);
        }
    }

    public void sendReceiverActionToClient(
            String routeId, MediaSink sink, String clientId, String action) {
        try {
            JSONObject jsonReceiver = new JSONObject();
            jsonReceiver.put("label", sink.getId());
            jsonReceiver.put("friendlyName", sink.getName());
            jsonReceiver.put("capabilities", toJSONArray(mSessionController.getCapabilities()));
            jsonReceiver.put("volume", null);
            jsonReceiver.put("isActiveInput", null);
            jsonReceiver.put("displayStatus", null);
            jsonReceiver.put("receiverType", "cast");

            JSONObject jsonReceiverAction = new JSONObject();
            jsonReceiverAction.put("receiver", jsonReceiver);
            jsonReceiverAction.put("action", action);

            JSONObject json = new JSONObject();
            json.put("type", "receiver_action");
            json.put("sequenceNumber", VOID_SEQUENCE_NUMBER);
            json.put("timeoutMillis", TIMEOUT_IMMEDIATE);
            json.put("clientId", clientId);
            json.put("message", jsonReceiverAction);

            mRouteProvider.sendMessageToClient(clientId, json.toString());
        } catch (JSONException e) {
            Log.e(TAG, "Failed to send receiver action message", e);
        }
    }

    /**
     * Sends a message to a specific client.
     * @param clientId The id of the receiving client.
     * @param type     The type of the message.
     * @param message  The message to be sent.
     * @param sequenceNumber The sequence number for matching requesting and responding messages.
     */
    public void sendEnclosedMessageToClient(
            String clientId, String type, String message, int sequenceNumber) {
        mRouteProvider.sendMessageToClient(
                clientId, buildEnclosedClientMessage(type, message, clientId, sequenceNumber));
    }

    @VisibleForTesting
    String buildEnclosedClientMessage(
            String type, String message, String clientId, int sequenceNumber) {
        JSONObject json = new JSONObject();
        try {
            json.put("type", type);
            json.put("sequenceNumber", sequenceNumber);
            json.put("timeoutMillis", TIMEOUT_IMMEDIATE);
            json.put("clientId", clientId);

            // TODO(mlamouri): we should have a more reliable way to handle string, null and Object
            // messages.
            if (message == null || "remove_session".equals(type)
                    || "disconnect_session".equals(type)) {
                json.put("message", message);
            } else {
                JSONObject jsonMessage = new JSONObject(message);
                if ("v2_message".equals(type)
                        && "MEDIA_STATUS".equals(jsonMessage.getString("type"))) {
                    sanitizeMediaStatusMessage(jsonMessage);
                }
                json.put("message", jsonMessage);
            }
        } catch (JSONException e) {
            Log.e(TAG, "Failed to build the reply: " + e);
        }

        return json.toString();
    }

    /**
     * @return A message containing the information of the {@link CastSession}.
     */
    public String buildSessionMessage() {
        if (!mSessionController.isConnected()) return "{}";

        try {
            // "volume" is a part of "receiver" initialized below.
            JSONObject jsonVolume = new JSONObject();
            jsonVolume.put("level", mSessionController.getSession().getVolume());
            jsonVolume.put("muted", mSessionController.getSession().isMute());

            // "receiver" is a part of "message" initialized below.
            JSONObject jsonReceiver = new JSONObject();
            jsonReceiver.put(
                    "label", mSessionController.getSession().getCastDevice().getDeviceId());
            jsonReceiver.put("friendlyName",
                    mSessionController.getSession().getCastDevice().getFriendlyName());
            jsonReceiver.put("capabilities", toJSONArray(mSessionController.getCapabilities()));
            jsonReceiver.put("volume", jsonVolume);
            jsonReceiver.put(
                    "isActiveInput", mSessionController.getSession().getActiveInputState());
            jsonReceiver.put("displayStatus", null);
            jsonReceiver.put("receiverType", "cast");

            JSONArray jsonNamespaces = new JSONArray();
            for (String namespace : mSessionController.getNamespaces()) {
                JSONObject jsonNamespace = new JSONObject();
                jsonNamespace.put("name", namespace);
                jsonNamespaces.put(jsonNamespace);
            }

            JSONObject jsonMessage = new JSONObject();
            jsonMessage.put("sessionId", mSessionController.getSessionId());
            jsonMessage.put("statusText", mSessionController.getSession().getApplicationStatus());
            jsonMessage.put("receiver", jsonReceiver);
            jsonMessage.put("namespaces", jsonNamespaces);
            jsonMessage.put("media", toJSONArray(new ArrayList<>()));
            jsonMessage.put("status", "connected");
            jsonMessage.put("transportId", "web-4");

            ApplicationMetadata applicationMetadata =
                    mSessionController.getSession().getApplicationMetadata();
            if (applicationMetadata != null) {
                jsonMessage.put("appId", applicationMetadata.getApplicationId());
            } else {
                jsonMessage.put("appId",
                        mSessionController.getRouteCreationInfo().source.getApplicationId());
            }
            jsonMessage.put("displayName",
                    mSessionController.getSession().getCastDevice().getFriendlyName());

            return jsonMessage.toString();
        } catch (JSONException e) {
            Log.w(TAG, "Building session message failed", e);
            return "{}";
        }
    }

    /////////////////////////////////////////////////////////////////////////////////////////////
    // Utility functions

    /**
     * Modifies the received MediaStatus message to match the format expected by the client.
     */
    private void sanitizeMediaStatusMessage(JSONObject object) throws JSONException {
        object.put("sessionId", mSessionController.getSessionId());

        JSONArray mediaStatus = object.getJSONArray("status");
        for (int i = 0; i < mediaStatus.length(); ++i) {
            JSONObject status = mediaStatus.getJSONObject(i);
            status.put("sessionId", mSessionController.getSessionId());
            if (!status.has("supportedMediaCommands")) continue;

            JSONArray commands = new JSONArray();
            int bitfieldCommands = status.getInt("supportedMediaCommands");
            for (int j = 0; j < 4; ++j) {
                if ((bitfieldCommands & (1 << j)) != 0) {
                    commands.put(MEDIA_SUPPORTED_COMMANDS[j]);
                }
            }

            status.put("supportedMediaCommands", commands); // Removes current entry.
        }
    }

    /**
     * Remove 'null' fields from a JSONObject. This method calls itself recursively until all the
     * fields have been looked at.
     * TODO(mlamouri): move to some util class?
     */
    private static void removeNullFields(Object object) throws JSONException {
        if (object instanceof JSONArray) {
            JSONArray array = (JSONArray) object;
            for (int i = 0; i < array.length(); ++i) removeNullFields(array.get(i));
        } else if (object instanceof JSONObject) {
            JSONObject json = (JSONObject) object;
            JSONArray names = json.names();
            if (names == null) return;
            for (int i = 0; i < names.length(); ++i) {
                String key = names.getString(i);
                if (json.isNull(key)) {
                    json.remove(key);
                } else {
                    removeNullFields(json.get(key));
                }
            }
        }
    }

    @VisibleForTesting
    boolean isMediaStatusMessage(String message) {
        try {
            JSONObject jsonMessage = new JSONObject(message);
            return "MEDIA_STATUS".equals(jsonMessage.getString("type"));
        } catch (JSONException e) {
            return false;
        }
    }

    private JSONArray toJSONArray(List<String> from) throws JSONException {
        JSONArray result = new JSONArray();
        for (String entry : from) {
            result.put(entry);
        }
        return result;
    }

    boolean sendStringCastMessage(
            String message, String namespace, String clientId, int sequenceNumber) {
        if (!mSessionController.isConnected()) return false;

        PendingResult<Status> pendingResult =
                mSessionController.getSession().sendMessage(namespace, message);
        if (!TextUtils.equals(namespace, CastSessionUtil.MEDIA_NAMESPACE)) {
            // Media commands wait for the media status update as a result.
            pendingResult.setResultCallback(
                    (Status result) -> onSendAppMessageResult(result, clientId, sequenceNumber));
        }
        return true;
    }

    /**
     * Notifies a client that an app message has been sent.
     * @param clientId The client id the message is sent from.
     * @param sequenceNumber The sequence number of the message.
     */
    void onSendAppMessageResult(Status result, String clientId, int sequenceNumber) {
        if (!result.isSuccess()) {
            // TODO(avayvod): should actually report back to the page.
            // See https://crbug.com/550445.
            Log.e(TAG, "Failed to send the message: " + result);
            return;
        }

        // App messages wait for the empty message with the sequence
        // number.
        sendEnclosedMessageToClient(clientId, "app_message", null, sequenceNumber);
    }
}
