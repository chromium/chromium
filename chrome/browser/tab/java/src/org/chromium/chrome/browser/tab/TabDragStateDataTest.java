// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link TabDragStateData}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabDragStateDataTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private Callback<Boolean> mCallback;

    private final UserDataHost mUserDataHost = new UserDataHost();

    @Before
    public void setUp() {
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
        when(mTab.isDestroyed()).thenReturn(false);
    }

    @Test
    public void testGetOrCreateForTab() {
        assertNull(TabDragStateData.getForTab(mTab));
        TabDragStateData data = TabDragStateData.getOrCreateForTab(mTab);
        assertNotNull(data);
        assertEquals(data, TabDragStateData.getForTab(mTab));
        assertFalse(data.getIsDraggingSupplier().get());
    }

    @Test
    public void testSetIsDragging() {
        TabDragStateData data = TabDragStateData.getOrCreateForTab(mTab);
        data.setIsDragging(true);
        assertTrue(data.getIsDraggingSupplier().get());
        data.setIsDragging(false);
        assertFalse(data.getIsDraggingSupplier().get());
    }

    @Test
    public void testObserverNotification() {
        TabDragStateData data = TabDragStateData.getOrCreateForTab(mTab);
        data.getIsDraggingSupplier().addSyncObserverAndPostIfNonNull(mCallback);

        data.setIsDragging(true);
        verify(mCallback).onResult(true);

        data.setIsDragging(false);
        verify(mCallback).onResult(false);
    }
}
