// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import org.chromium.components.bookmarks.BookmarkId;

import java.util.UUID;

/** Object to hold a client identifier for an offline page. */
public class ClientId {
    private String mNamespace;
    private String mId;

    public ClientId(String namespace, String id) {
        mNamespace = namespace;
        mId = id;
    }

    public String getNamespace() {
        return mNamespace;
    }

    public String getId() {
        return mId;
    }

    @Override
    public boolean equals(Object o) {
        if (o instanceof ClientId) {
            ClientId otherId = (ClientId) o;
            return otherId.getNamespace().equals(mNamespace) && otherId.getId().equals(mId);
        }
        return false;
    }

    @Override
    public int hashCode() {
        return (mNamespace + ":" + mId).hashCode();
    }

    /**
     * Create a client id for a bookmark
     * @param id The bookmark id to wrap.
     * @return A {@link ClientId} that represents this BookmarkId.
     */
    public static ClientId createClientIdForBookmarkId(BookmarkId id) {
        if (id == null) return null;
        return new ClientId(OfflinePageBridge.BOOKMARK_NAMESPACE, id.toString());
    }

    /**
     * Create a client id for a namespace.
     * @param namespace The namespace for the client id.
     * @return A {@link ClientId} for this namespace with generated UUID.
     */
    public static ClientId createGuidClientIdForNamespace(String namespace) {
        String uuid = UUID.randomUUID().toString();
        return new ClientId(namespace, uuid);
    }
}
