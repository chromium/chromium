// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.bookmarks.BookmarkType;

/** Unit tests for ClientId. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ClientIdTest {
    private static final long INVALID_BOOKMARK_ID = -1;
    private static final long TEST_BOOKMARK_ID = 42;

    private static final String TEST_NAMESPACE = "TEST_NAMESPACE";
    private static final String TEST_ID = "TEST_ID";

    /**
     * Tests ClientId#createClientIdForBookmarkId() method in cases with valid, invalid and null
     * bookmark ID.
     */
    @Test
    @Feature({"OfflinePages"})
    public void testCreateClientIdForBookmarkId() {
        ClientId clientId =
                ClientId.createClientIdForBookmarkId(
                        new BookmarkId(TEST_BOOKMARK_ID, BookmarkType.NORMAL));
        assertNotNull(clientId);
        assertEquals(OfflinePageBridge.BOOKMARK_NAMESPACE, clientId.getNamespace());
        assertEquals(Long.toString(TEST_BOOKMARK_ID), clientId.getId());

        clientId =
                ClientId.createClientIdForBookmarkId(
                        new BookmarkId(INVALID_BOOKMARK_ID, BookmarkType.NORMAL));
        assertNotNull(clientId);
        assertEquals(OfflinePageBridge.BOOKMARK_NAMESPACE, clientId.getNamespace());
        assertEquals(Long.toString(INVALID_BOOKMARK_ID), clientId.getId());

        clientId = ClientId.createClientIdForBookmarkId(null);
        assertNull(clientId);
    }

    /** Ensure that ClientId works properly. */
    @Test
    @Feature({"OfflinePages"})
    public void testClientIdConstructor() {
        ClientId clientId = new ClientId(TEST_NAMESPACE, TEST_ID);
        assertEquals(TEST_NAMESPACE, clientId.getNamespace());
        assertEquals(TEST_ID, clientId.getId());
    }
}
