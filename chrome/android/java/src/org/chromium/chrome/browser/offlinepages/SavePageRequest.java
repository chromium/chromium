// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

/** Data class representing an underlying request to save a page later. */
@JNINamespace("offline_pages::android")
public class SavePageRequest {
    // Int representation of the org.chromium.components.offlinepages.RequestState enum.
    private int mRequestState;
    private long mRequestId;
    private String mUrl;
    private ClientId mClientId;
    private OfflinePageOrigin mOrigin;
    // Int representation of SavePageRequest::AutoFetchNotificationState
    private int mAutoFetchNotificationState;

    /**
     * Creates a SavePageRequest that's a copy of the C++ side version.
     *
     * <p>NOTE: This does not mirror all fields so it cannot be used to create a full
     * SavePageRequest on its own.
     *
     * @param state Result of the saving. Uses {@see
     *     org.chromium.components.offlinepages.RequestState} enum.
     * @param requestId The unique ID of the request.
     * @param url The URL to download
     * @param clientIdNamespace a String that will be the namespace of the client ID of this
     *     request.
     * @param clientIdId a String that will be the ID of the client ID of this request.
     */
    @CalledByNative
    public static SavePageRequest create(
            int state,
            long requestId,
            @JniType("std::string") String url,
            @JniType("std::string") String clientIdNamespace,
            @JniType("std::string") String clientIdId,
            @JniType("std::string") String originString,
            int autoFetchNotificationState) {
        return new SavePageRequest(
                state,
                requestId,
                url,
                new ClientId(clientIdNamespace, clientIdId),
                new OfflinePageOrigin(originString),
                autoFetchNotificationState);
    }

    private SavePageRequest(
            int state,
            long requestId,
            String url,
            ClientId clientId,
            OfflinePageOrigin origin,
            int autoFetchNotificationState) {
        mRequestState = state;
        mRequestId = requestId;
        mUrl = url;
        mClientId = clientId;
        mOrigin = origin;
        mAutoFetchNotificationState = autoFetchNotificationState;
    }

    public int getRequestState() {
        return mRequestState;
    }

    public long getRequestId() {
        return mRequestId;
    }

    public String getUrl() {
        return mUrl;
    }

    public ClientId getClientId() {
        return mClientId;
    }

    public OfflinePageOrigin getOrigin() {
        return mOrigin;
    }

    public int getAutoFetchNotificationState() {
        return mAutoFetchNotificationState;
    }
}
