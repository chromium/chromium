// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.StorageLoadedData.LoadedTabState;
import org.chromium.chrome.browser.tab.StorageLoadedData.StorageLoadWarning;

/** Unit tests for {@link StorageLoadedData} and {@link LoadedTabState}. */
@RunWith(BaseRobolectricTestRunner.class)
public class StorageLoadedDataUnitTest {
    private static final long NATIVE_PTR = 12345L;

    @Rule public final MockitoRule mMockito = MockitoJUnit.rule();

    @Mock private StorageLoadedData.Natives mStorageLoadedDataJniMock;
    @Mock private WebContentsState mWebContentsState;
    @Mock private WebContentsState mContentsStateClaimed;
    @Mock private WebContentsState mContentsStateUnclaimed;
    @Mock private TabGroupCollectionData mTabGroupCollectionData;

    @Before
    public void setUp() {
        StorageLoadedDataJni.setInstanceForTesting(mStorageLoadedDataJniMock);
    }

    @Test
    public void testLoadedTabState_Claim() {
        TabState tabState = new TabState();
        LoadedTabState loadedTabState = new LoadedTabState(1, tabState);

        assertFalse(loadedTabState.isClaimedOrDestroyed());

        TabState claimed = loadedTabState.claim();
        assertEquals(tabState, claimed);
        assertTrue(loadedTabState.isClaimedOrDestroyed());

        // Subsequent claim calls must return null.
        assertNull(loadedTabState.claim());
        assertTrue(loadedTabState.isClaimedOrDestroyed());
    }

    @Test
    public void testLoadedTabState_DestroyUnclaimed() {
        TabState tabState = new TabState();
        tabState.contentsState = mWebContentsState;
        LoadedTabState loadedTabState = new LoadedTabState(1, tabState);

        assertFalse(loadedTabState.isClaimedOrDestroyed());

        loadedTabState.destroy();

        assertTrue(loadedTabState.isClaimedOrDestroyed());
        verify(mWebContentsState).destroy();
        assertNull(tabState.contentsState);

        // Subsequent claim calls must return null after destruction.
        assertNull(loadedTabState.claim());
    }

    @Test
    public void testLoadedTabState_DestroyClaimedPreservesWebContentsState() {
        TabState tabState = new TabState();
        tabState.contentsState = mWebContentsState;
        LoadedTabState loadedTabState = new LoadedTabState(1, tabState);

        TabState claimed = loadedTabState.claim();
        assertNotNull(claimed);
        assertTrue(loadedTabState.isClaimedOrDestroyed());

        loadedTabState.destroy();

        assertTrue(loadedTabState.isClaimedOrDestroyed());
        verify(mWebContentsState, never()).destroy();
        assertEquals(mWebContentsState, tabState.contentsState);
    }

    @Test
    public void testLoadedTabState_DestroyWithNullContentsState() {
        TabState tabState = new TabState();
        tabState.contentsState = null;
        LoadedTabState loadedTabState = new LoadedTabState(1, tabState);

        // Should not throw or crash.
        loadedTabState.destroy();
        assertTrue(loadedTabState.isClaimedOrDestroyed());
        assertNull(tabState.contentsState);
    }

    @Test
    public void testStorageLoadedData_DestroyCleansUnclaimedOnly() {
        TabState tabStateClaimed = new TabState();
        tabStateClaimed.contentsState = mContentsStateClaimed;
        LoadedTabState loadedStateClaimed = new LoadedTabState(1, tabStateClaimed);
        loadedStateClaimed.claim();

        TabState tabStateUnclaimed = new TabState();
        tabStateUnclaimed.contentsState = mContentsStateUnclaimed;
        LoadedTabState loadedStateUnclaimed = new LoadedTabState(2, tabStateUnclaimed);

        TabState tabStateNullContents = new TabState();
        tabStateNullContents.contentsState = null;
        LoadedTabState loadedStateNullContents = new LoadedTabState(3, tabStateNullContents);

        StorageLoadedData storageLoadedData =
                StorageLoadedData.createData(
                        NATIVE_PTR,
                        new LoadedTabState[] {
                            loadedStateClaimed, loadedStateUnclaimed, loadedStateNullContents
                        },
                        new TabGroupCollectionData[] {mTabGroupCollectionData},
                        /* activeTabIndex= */ 0,
                        new StorageLoadWarning[0]);

        storageLoadedData.destroy();

        // Claimed state must not be destroyed.
        verify(mContentsStateClaimed, never()).destroy();
        assertEquals(mContentsStateClaimed, tabStateClaimed.contentsState);
        assertTrue(loadedStateClaimed.isClaimedOrDestroyed());

        // Unclaimed state must be destroyed and nulled out.
        verify(mContentsStateUnclaimed).destroy();
        assertNull(tabStateUnclaimed.contentsState);
        assertTrue(loadedStateUnclaimed.isClaimedOrDestroyed());

        // Null contents state must be marked claimed or destroyed.
        assertTrue(loadedStateNullContents.isClaimedOrDestroyed());

        // Group data and native pointer must be destroyed.
        verify(mTabGroupCollectionData).destroy();
        verify(mStorageLoadedDataJniMock).destroy(NATIVE_PTR);
        assertEquals(0, storageLoadedData.getNativePtr());
    }
}
