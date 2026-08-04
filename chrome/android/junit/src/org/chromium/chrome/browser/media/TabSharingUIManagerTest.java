// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.WebContents;

/** Unit tests for {@link TabSharingUIManager}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSharingUIManagerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabSharingUIManager.Observer mObserver1;
    @Mock private TabSharingUIManager.Observer mObserver2;
    @Mock private TabSharingUIBridge mBridge1;
    @Mock private WebContents mCapturer1;
    @Mock private WebContents mCapturer2;

    private TabSharingUIManager mManager;

    @Before
    public void setUp() {
        mManager = new TabSharingUIManager();
        TabSharingUIManager.setInstanceForTesting(mManager);
    }

    @After
    public void tearDown() throws Exception {
        // Clean up observers.
        mManager.removeObserver(mObserver1);
        mManager.removeObserver(mObserver2);
    }

    @Test
    public void testAddRemoveObserver() {
        mManager.addObserver(mObserver1);
        mManager.addBridge(mBridge1);
        verify(mObserver1).onSharingSessionStarted(mBridge1);

        mManager.removeObserver(mObserver1);
        mManager.removeBridge(mBridge1);
        verifyNoMoreInteractions(mObserver1);
    }

    @Test
    public void testMultipleObservers() {
        mManager.addObserver(mObserver1);
        mManager.addObserver(mObserver2);

        mManager.addBridge(mBridge1);
        verify(mObserver1).onSharingSessionStarted(mBridge1);
        verify(mObserver2).onSharingSessionStarted(mBridge1);

        mManager.removeBridge(mBridge1);
        verify(mObserver1).onSharingSessionStopped(mBridge1);
        verify(mObserver2).onSharingSessionStopped(mBridge1);
    }

    @Test
    public void testAddObserverWithExistingBridges() {
        mManager.addBridge(mBridge1);

        // Observer 1 added after bridge 1 is already active.
        // It should be notified immediately.
        mManager.addObserver(mObserver1);
        verify(mObserver1).onSharingSessionStarted(mBridge1);
    }

    @Test(expected = AssertionError.class)
    public void testRemoveBridgeNotFoundAsserts() {
        // This should assert because mBridge1 was never added.
        mManager.removeBridge(mBridge1);
    }

    @Test
    public void testStopSharingByCapturerTab() {
        when(mBridge1.getCapturer()).thenReturn(mCapturer1);
        mManager.addBridge(mBridge1);

        // Stopping an unrelated capturer should do nothing.
        mManager.stopSharingByCapturerTab(mCapturer2);
        verify(mBridge1, org.mockito.Mockito.never()).stopSharing();

        // Stopping the registered capturer should invoke stopSharing().
        mManager.stopSharingByCapturerTab(mCapturer1);
        verify(mBridge1).stopSharing();
    }

    @Test
    public void testIsSharing() {
        assertFalse(mManager.isSharing());
        mManager.addBridge(mBridge1);
        assertTrue(mManager.isSharing());
        mManager.removeBridge(mBridge1);
        assertFalse(mManager.isSharing());
    }
}
