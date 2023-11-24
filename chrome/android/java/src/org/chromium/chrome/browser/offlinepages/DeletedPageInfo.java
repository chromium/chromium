// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

/** Simple object representing important information of a deleted offline page. */
public class DeletedPageInfo {
    private final long mOfflineId;
    private final ClientId mClientId;
    private final String mRequestOrigin;

    public DeletedPageInfo(
            long offlineId, String clientNamespace, String clientId, String requestOrigin) {
        this(offlineId, new ClientId(clientNamespace, clientId), requestOrigin);
    }

    public DeletedPageInfo(long offlineId, ClientId clientId, String requestOrigin) {
        mOfflineId = offlineId;
        mClientId = clientId;
        mRequestOrigin = requestOrigin;
    }

    /** @return offline id for the deleted page */
    public long getOfflineId() {
        return mOfflineId;
    }

    /** @return Client Id for the deleted page */
    public ClientId getClientId() {
        return mClientId;
    }

    /** @return request origin of the deleted page */
    public String getRequestOrigin() {
        return mRequestOrigin;
    }
}
