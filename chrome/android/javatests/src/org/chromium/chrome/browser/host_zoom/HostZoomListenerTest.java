// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.host_zoom;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotEquals;
import static org.mockito.ArgumentMatchers.anyDouble;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.accessibility.ZoomEventsObserver;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.HostZoomMap;

import java.util.concurrent.TimeoutException;

/** Integration tests for {@link HostZoomListener}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class HostZoomListenerTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock private ZoomEventsObserver mObserver;
    @Captor private ArgumentCaptor<String> mHostCaptor;
    @Captor private ArgumentCaptor<Double> mZoomLevelCaptor;

    private HostZoomListener mHostZoomListener;
    private BrowserContextHandle mBrowserContextHandle;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        runOnUiThreadBlocking(
                () -> {
                    mBrowserContextHandle =
                            Profile.fromWebContents(mActivityTestRule.getWebContents());
                    mHostZoomListener = new HostZoomListener(mBrowserContextHandle);
                });
    }

    @After
    public void tearDown() {
        runOnUiThreadBlocking(
                () -> {
                    if (mHostZoomListener != null) {
                        mHostZoomListener.destroy();
                    }
                });
    }

    @Test
    @SmallTest
    public void testNativeObserverIsSetCorrectly() {
        runOnUiThreadBlocking(
                () -> {
                    assertNotEquals(
                            "Native listener should be set on creation.",
                            0,
                            mHostZoomListener.getNativeHostZoomLevelListenerKeyForTesting());
                });
    }

    @Test
    @SmallTest
    public void testListenerIsNotifiedOnZoomChange() throws TimeoutException {
        final String host = "example.com";
        final double zoomLevel = 2.0;
        final CallbackHelper callbackHelper = new CallbackHelper();

        doAnswer(
                        invocation -> {
                            callbackHelper.notifyCalled();
                            return null;
                        })
                .when(mObserver)
                .onZoomLevelChanged(anyString(), anyDouble());

        runOnUiThreadBlocking(
                () -> {
                    mHostZoomListener.addObserver(mObserver);
                    HostZoomMap.setZoomLevelForHost(mBrowserContextHandle, host, zoomLevel);
                });

        callbackHelper.waitForCallback(0);

        verify(mObserver, times(1))
                .onZoomLevelChanged(mHostCaptor.capture(), mZoomLevelCaptor.capture());
        assertEquals("Host should match.", host, mHostCaptor.getValue());
        assertEquals("Zoom level should match.", zoomLevel, mZoomLevelCaptor.getValue(), 0.0);
    }

    @Test
    @SmallTest
    public void testListenerIsRemovedCorrectly() {
        final String host = "example.com";
        final double zoomLevel = 2.5;

        // Add and then immediately remove the observer.
        runOnUiThreadBlocking(
                () -> {
                    mHostZoomListener.addObserver(mObserver);
                    mHostZoomListener.removeObserver(mObserver);
                    HostZoomMap.setZoomLevelForHost(mBrowserContextHandle, host, zoomLevel);
                });

        // Verify the observer was not called.
        verify(mObserver, never()).onZoomLevelChanged(anyString(), anyDouble());
    }

    @Test
    @SmallTest
    public void testListenerRemovedOnDestroy() {
        final String host = "example.com";
        final double zoomLevel = 3.0;

        runOnUiThreadBlocking(
                () -> {
                    mHostZoomListener.addObserver(mObserver);
                    mHostZoomListener.destroy();

                    assertEquals(
                            "Native listener should be cleared after destroy.",
                            0,
                            mHostZoomListener.getNativeHostZoomLevelListenerKeyForTesting());

                    // This change should not be propagated.
                    HostZoomMap.setZoomLevelForHost(mBrowserContextHandle, host, zoomLevel);
                });

        // Verify the observer was not called.
        verify(mObserver, never()).onZoomLevelChanged(anyString(), anyDouble());

        // Set manager to null to avoid double-destroy in tearDown.
        mHostZoomListener = null;
    }
}
