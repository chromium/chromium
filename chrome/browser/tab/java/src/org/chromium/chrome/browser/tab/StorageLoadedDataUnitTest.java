// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;

import org.jni_zero.JniUniquePtr;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.StorageLoadedData.LoadedTabState;
import org.chromium.chrome.browser.tab.StorageLoadedData.StorageLoadWarning;

import java.nio.ByteBuffer;

/** Unit tests for {@link StorageLoadedData} and {@link LoadedTabState}. */
@RunWith(BaseRobolectricTestRunner.class)
public class StorageLoadedDataUnitTest {
    @Test
    public void testStorageLoadedData_Destroy() {
        LoadedTabState loadedState1 = new LoadedTabState(1, new TabState());
        LoadedTabState loadedState2 = new LoadedTabState(2, new TabState());

        TabGroupCollectionData groupData = mock(TabGroupCollectionData.class);

        StorageLoadedData storageLoadedData =
                StorageLoadedData.createData(
                        JniUniquePtr.createForTesting(12345L),
                        new LoadedTabState[] {loadedState1, loadedState2},
                        new TabGroupCollectionData[] {groupData},
                        /* activeTabIndex= */ 0,
                        new StorageLoadWarning[0]);

        storageLoadedData.destroy();

        // Native pointer must be destroyed.
        assertNull(storageLoadedData.getNativePtr());
    }

    @Test
    public void testCreateTabState() {
        ByteBuffer buffer = ByteBuffer.allocateDirect(4);
        TabState state =
                StorageLoadedData.createTabState(
                        /* parentTabId= */ 1,
                        /* timestampMillis= */ 100L,
                        buffer,
                        WebContentsState.CONTENTS_STATE_CURRENT_VERSION,
                        /* openerAppId= */ "app",
                        /* themeColor= */ 0,
                        TabLaunchType.FROM_LINK,
                        TabUserAgent.DEFAULT,
                        /* lastNavigationCommittedTimestampMillis= */ 200L,
                        /* tabGroupId= */ null,
                        /* tabHasSensitiveContent= */ false,
                        /* isPinned= */ true,
                        "https://example.com");
        assertNotNull(state.contentsState);
        assertEquals(buffer, state.contentsState.buffer());
        assertEquals(
                WebContentsState.CONTENTS_STATE_CURRENT_VERSION, state.contentsState.version());
        assertNotNull(state.url);
        assertEquals("https://example.com/", state.url.getSpec());

        TabState emptyState =
                StorageLoadedData.createTabState(
                        /* parentTabId= */ Tab.INVALID_TAB_ID,
                        /* timestampMillis= */ 0L,
                        /* webContentsStateBuffer= */ null,
                        WebContentsState.INVALID_BUFFER_VERSION,
                        /* openerAppId= */ null,
                        /* themeColor= */ 0,
                        TabLaunchType.UNSET,
                        TabUserAgent.DEFAULT,
                        /* lastNavigationCommittedTimestampMillis= */ 0L,
                        /* tabGroupId= */ null,
                        /* tabHasSensitiveContent= */ false,
                        /* isPinned= */ false,
                        "not a valid url");
        assertNull(emptyState.contentsState);
        assertNull(emptyState.url);
    }
}
