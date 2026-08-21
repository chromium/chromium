// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.mockito.Mockito.withSettings;

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

/** Unit tests for {@link MediaCaptureDevicesDispatcherAndroid}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MediaCaptureDevicesDispatcherAndroidTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private WebContents mWebContents;
    @Mock private MediaCaptureDevicesDispatcherAndroid.Observer mObserver;
    @Mock private MediaCaptureDevicesDispatcherAndroid.Natives mNativeMock;

    @Before
    public void setUp() {
        MediaCaptureDevicesDispatcherAndroidJni.setInstanceForTesting(mNativeMock);
        mWebContents =
                mock(
                        WebContents.class,
                        withSettings().extraInterfaces(WebContentsObserver.Observable.class));
        MediaCaptureDevicesDispatcherAndroid.addObserver(mObserver);
    }

    @After
    public void tearDown() {
        MediaCaptureDevicesDispatcherAndroid.removeObserver(mObserver);
        MediaCaptureDevicesDispatcherAndroid.setSourceSwitchingInProgress(mWebContents, false);
    }

    @Test
    public void testNormalCaptureEvents() {
        MediaCaptureDevicesDispatcherAndroid.onIsCapturingTabChanged(mWebContents, true);
        verify(mObserver).onIsCapturingTabChanged(mWebContents, true);

        MediaCaptureDevicesDispatcherAndroid.onIsCapturingTabChanged(mWebContents, false);
        verify(mObserver).onIsCapturingTabChanged(mWebContents, false);
    }

    @Test
    public void testSourceSwitchingSuppressesTransientStop() {
        MediaCaptureDevicesDispatcherAndroid.setSourceSwitchingInProgress(mWebContents, true);

        // While switching source, transient stop event should be cleanly suppressed.
        MediaCaptureDevicesDispatcherAndroid.onIsCapturingTabChanged(mWebContents, false);
        verify(mObserver, never()).onIsCapturingTabChanged(mWebContents, false);

        // When the new stream starts, true event should be emitted and clears switching state.
        when(mNativeMock.isCapturingTab(mWebContents)).thenReturn(true);
        MediaCaptureDevicesDispatcherAndroid.onIsCapturingTabChanged(mWebContents, true);
        verify(mObserver).onIsCapturingTabChanged(mWebContents, true);

        // Subsequent stops (after switch finishes) should be forwarded normally.
        when(mNativeMock.isCapturingTab(mWebContents)).thenReturn(false);
        MediaCaptureDevicesDispatcherAndroid.onIsCapturingTabChanged(mWebContents, false);
        verify(mObserver).onIsCapturingTabChanged(mWebContents, false);
    }

    @Test
    public void testCapturerDestroyed_ClearsSwitchingState() {
        WebContents mockWebContents =
                mock(
                        WebContents.class,
                        withSettings().extraInterfaces(WebContentsObserver.Observable.class));

        ArgumentCaptor<WebContentsObserver> captor =
                ArgumentCaptor.forClass(WebContentsObserver.class);

        MediaCaptureDevicesDispatcherAndroid.setSourceSwitchingInProgress(mockWebContents, true);
        assertTrue(
                MediaCaptureDevicesDispatcherAndroid.isSourceSwitchingInProgress(mockWebContents));
        verify((WebContentsObserver.Observable) mockWebContents).addObserver(captor.capture());

        // When capturer is destroyed mid-switch, state is cleared.
        captor.getValue().webContentsDestroyed();
        assertFalse(
                MediaCaptureDevicesDispatcherAndroid.isSourceSwitchingInProgress(mockWebContents));
    }

    @Test
    public void testAbortedSourceSwitch_ReplaysSuppressedStop() {
        MediaCaptureDevicesDispatcherAndroid.setSourceSwitchingInProgress(mWebContents, true);

        // While switching source, transient stop event is suppressed.
        MediaCaptureDevicesDispatcherAndroid.onIsCapturingTabChanged(mWebContents, false);
        verify(mObserver, never()).onIsCapturingTabChanged(mWebContents, false);

        // If the switch aborts/times out without a new stream starting, clearing switching state
        // replays the suppressed stop event to observers.
        MediaCaptureDevicesDispatcherAndroid.setSourceSwitchingInProgress(mWebContents, false);
        verify(mObserver).onIsCapturingTabChanged(mWebContents, false);
    }
}
