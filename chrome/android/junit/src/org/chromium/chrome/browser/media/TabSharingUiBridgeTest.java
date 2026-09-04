// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;

/** Unit tests for {@link TabSharingUiBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSharingUiBridgeTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabSharingUiBridge.Natives mNativeMock;

    @Mock(extraInterfaces = {WebContentsObserver.Observable.class})
    private WebContents mCapturer;

    @Mock(extraInterfaces = {WebContentsObserver.Observable.class})
    private WebContents mCapturee;

    @Mock private TabSharingUiManager.Observer mManagerObserver;
    @Mock private MediaCaptureDevicesDispatcherAndroid.Natives mMediaCaptureJniMock;

    private static final long NATIVE_PTR = 12345L;
    private TabSharingUiBridge mBridge;

    @Before
    public void setUp() {
        TabSharingUiBridgeJni.setInstanceForTesting(mNativeMock);
        MediaCaptureDevicesDispatcherAndroidJni.setInstanceForTesting(mMediaCaptureJniMock);
        TabSharingUiManager.getInstance().addObserver(mManagerObserver);
        mBridge = TabSharingUiBridge.create(NATIVE_PTR, mCapturer, mCapturee, true, false);
    }

    @After
    public void tearDown() {
        TabSharingUiBridgeJni.setInstanceForTesting(null);
        if (mBridge != null) {
            mBridge.destroy();
            mBridge = null;
        }
        TabSharingUiManager.getInstance().removeObserver(mManagerObserver);
    }

    @Test
    public void testCreateAndDestroy() {
        assertNotNull(mBridge);
        assertEquals(mCapturer, mBridge.getCapturer());
        assertEquals(mCapturee, mBridge.getCapturee());
        verify(mManagerObserver).onSharingSessionStarted(mBridge);

        mBridge.destroy();
        verify(mManagerObserver).onSharingSessionStopped(mBridge);
        mBridge = null;
    }

    @Test
    public void testStopSharing() {
        mBridge.stopSharing();
        verify(mNativeMock).stopSharing(NATIVE_PTR);
    }

    @Test
    public void testChangeSource() {
        mBridge.changeSource(mCapturee);
        verify(mNativeMock).changeSource(NATIVE_PTR, mCapturee);
    }

    @Test
    public void testWebContentsDestroyedTriggersStopSharing() {
        ArgumentCaptor<WebContentsObserver> captor =
                ArgumentCaptor.forClass(WebContentsObserver.class);
        verify((WebContentsObserver.Observable) mCapturer).addObserver(captor.capture());
        verify((WebContentsObserver.Observable) mCapturee).addObserver(any());

        WebContentsObserver observer = captor.getValue();
        assertNotNull(observer);

        // Simulating capturer destruction should trigger stopSharing().
        observer.webContentsDestroyed();
        verify(mNativeMock).stopSharing(NATIVE_PTR);
    }

    @Test
    public void testCapabilityGettersAndPostDestroySafety() {
        assertTrue(mBridge.isSourceSwitchingSupported());
        assertFalse(mBridge.appPreferredCurrentTab());

        mBridge.destroy();
        mBridge.stopSharing();
        mBridge.changeSource(mCapturee);
        mBridge = null;
    }
}
