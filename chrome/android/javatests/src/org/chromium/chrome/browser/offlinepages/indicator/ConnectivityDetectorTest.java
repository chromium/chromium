// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.NativeLibraryTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;

/**
 * Tests for {@link ConnectivityDetector}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class ConnectivityDetectorTest implements ConnectivityDetector.Observer {
    private static final int TIMEOUT_MS = 5000;

    private ConnectivityDetector mConnectivityDetector;
    private @ConnectivityDetector.ConnectionState int mConnectionState =
            ConnectivityDetector.ConnectionState.NONE;
    private Semaphore mSemaphore = new Semaphore(0);
    // This test focuses on testing ConnectivityDetector functionalities, including http probes.
    // So don't skip http probes here.
    private ConnectivityDetectorDelegateStub mConnectivityDetectorDelegate =
            new ConnectivityDetectorDelegateStub(ConnectivityDetector.ConnectionState.NO_INTERNET,
                    false /*shouldSkipHttpProbes*/);

    @Rule
    public NativeLibraryTestRule mNativeLibraryTestRule = new NativeLibraryTestRule();

    @Before
    public void setUp() throws Exception {
        mNativeLibraryTestRule.loadNativeLibraryNoBrowserProcess();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            if (!NetworkChangeNotifier.isInitialized()) {
                NetworkChangeNotifier.init();
            }
            NetworkChangeNotifier.forceConnectivityState(true);
            ConnectivityDetector.overrideConnectivityCheckInitialDelayMs(1000);
            ConnectivityDetector.setDelegateForTesting(mConnectivityDetectorDelegate);
            mConnectivityDetector = new ConnectivityDetector(this);
            mConnectivityDetectorDelegate.setConnectionStateFromSystem(
                    ConnectivityDetector.ConnectionState.NONE);
        });
        // Wait until the initial detection logic finishes to give all tests the same starting
        // point.
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.NO_INTERNET, mConnectionState);
    }

    @Override
    public void onConnectionStateChanged(
            @ConnectivityDetector.ConnectionState int connectionState) {
        mConnectionState = connectionState;
        mSemaphore.release();
    }

    @Test
    @MediumTest
    public void testEvaulateConnectionStateBySystem() throws Exception {
        setNetworkConnectivity(false);
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.DISCONNECTED, mConnectionState);

        mConnectivityDetectorDelegate.setConnectionStateFromSystem(
                ConnectivityDetector.ConnectionState.CAPTIVE_PORTAL);
        setNetworkConnectivity(true);
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.CAPTIVE_PORTAL, mConnectionState);
    }

    @Test
    @MediumTest
    public void testReevaulateConnectionStateBySystem() throws Exception {
        setNetworkConnectivity(false);
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.DISCONNECTED, mConnectionState);

        mConnectivityDetectorDelegate.setConnectionStateFromSystem(
                ConnectivityDetector.ConnectionState.CAPTIVE_PORTAL);
        setNetworkConnectivity(true);
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.CAPTIVE_PORTAL, mConnectionState);

        // detect() should get the connection info from the system again.
        mConnectivityDetectorDelegate.setConnectionStateFromSystem(
                ConnectivityDetector.ConnectionState.VALIDATED);
        mConnectivityDetector.detect();
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.VALIDATED, mConnectionState);
    }

    @Test
    @MediumTest
    public void testReevaulateConnectionStateBySystemAfterBackoff() throws Exception {
        setNetworkConnectivity(false);
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.DISCONNECTED, mConnectionState);

        mConnectivityDetectorDelegate.setConnectionStateFromSystem(
                ConnectivityDetector.ConnectionState.CAPTIVE_PORTAL);
        setNetworkConnectivity(true);
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.CAPTIVE_PORTAL, mConnectionState);

        // We should get the connection info from the system again after some delay.
        mConnectivityDetectorDelegate.setConnectionStateFromSystem(
                ConnectivityDetector.ConnectionState.VALIDATED);
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.VALIDATED, mConnectionState);
    }

    @Test
    @MediumTest
    public void testProbeDefaultUrlReturning204() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL("/nocontent");

        ConnectivityDetector.overrideDefaultProbeUrlForTesting(testUrl);
        checkConnectivityViaDefaultUrl();
        Assert.assertEquals(ConnectivityDetector.ConnectionState.VALIDATED, mConnectionState);
    }

    @Test
    @MediumTest
    public void testProbeDefaultUrlReturning200WithoutContent() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL("/echo?status=200");

        ConnectivityDetector.overrideDefaultProbeUrlForTesting(testUrl);
        // This will make the test sever return empty content.
        ConnectivityDetector.overrideProbeMethodForTesting("POST");
        checkConnectivityViaDefaultUrl();
        Assert.assertEquals(ConnectivityDetector.ConnectionState.VALIDATED, mConnectionState);
    }

    @Test
    @MediumTest
    public void testProbeDefaultUrlReturning200WithContent() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL("/echo?status=200");

        ConnectivityDetector.overrideDefaultProbeUrlForTesting(testUrl);
        checkConnectivityViaDefaultUrl();
        Assert.assertEquals(ConnectivityDetector.ConnectionState.CAPTIVE_PORTAL, mConnectionState);
    }

    @Test
    @MediumTest
    public void testProbeDefaultUrlReturning304() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL("/echo?status=304");

        ConnectivityDetector.overrideDefaultProbeUrlForTesting(testUrl);
        checkConnectivityViaDefaultUrl();
        Assert.assertEquals(ConnectivityDetector.ConnectionState.CAPTIVE_PORTAL, mConnectionState);
    }

    @Test
    @MediumTest
    public void testProbeDefaultUrlReturning500() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL("/echo?status=500");
        String noContentUrl = testServer.getURL("/nocontent");

        ConnectivityDetector.overrideDefaultProbeUrlForTesting(testUrl);
        ConnectivityDetector.overrideFallbackProbeUrlForTesting(noContentUrl);
        checkConnectivityViaDefaultUrl();
        // Probing default URL gets back 500 which should cause no change to connection state but
        // probing fallback URL immediately. We should then trigger VALIDATED state.
        Assert.assertEquals(ConnectivityDetector.ConnectionState.VALIDATED, mConnectionState);
        Assert.assertFalse(hasScheduledRetry());
    }

    @Test
    @MediumTest
    public void testProbeFallbackUrlReturning204() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL("/nocontent");

        ConnectivityDetector.overrideFallbackProbeUrlForTesting(testUrl);
        checkConnectivityViaFallbackUrl();
        Assert.assertEquals(ConnectivityDetector.ConnectionState.VALIDATED, mConnectionState);
        Assert.assertFalse(hasScheduledRetry());
    }

    @Test
    @MediumTest
    public void testProbeFallbackUrlReturning304() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL("/echo?status=304");

        ConnectivityDetector.overrideFallbackProbeUrlForTesting(testUrl);
        checkConnectivityViaFallbackUrl();
        Assert.assertEquals(ConnectivityDetector.ConnectionState.CAPTIVE_PORTAL, mConnectionState);
        Assert.assertTrue(hasScheduledRetry());
    }

    @Test
    @MediumTest
    public void testDetectDisconnectedState() throws Exception {
        setNetworkConnectivity(false);
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.DISCONNECTED, mConnectionState);
    }

    @Test
    @MediumTest
    public void testDetectValidatedState() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String testUrl = testServer.getURL("/nocontent");
        ConnectivityDetector.overrideDefaultProbeUrlForTesting(testUrl);

        setNetworkConnectivity(false);
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.DISCONNECTED, mConnectionState);
        setNetworkConnectivity(true);
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.VALIDATED, mConnectionState);
    }

    @Test
    @MediumTest
    public void testDetectCaptivePortalAndThenValidatedState() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String hasContentUrl = testServer.getURL("/echo?status=200");
        ConnectivityDetector.overrideDefaultProbeUrlForTesting(hasContentUrl);
        String noContentUrl = testServer.getURL("/nocontent");
        ConnectivityDetector.overrideFallbackProbeUrlForTesting(noContentUrl);

        setNetworkConnectivity(false);
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.DISCONNECTED, mConnectionState);
        setNetworkConnectivity(true);
        // Probing default URL gets back 200 which should trigger CAPTIVE_PORTAL state.
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.CAPTIVE_PORTAL, mConnectionState);
        // Probing fallback URL immediately after default URL gets back "204 no content" which
        // should trigger VALIDATED state.
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.VALIDATED, mConnectionState);
    }

    @Test
    @MediumTest
    public void testDetectCaptivePortalStateForBothDefaultAndFallbackUrls() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String hasContentUrl = testServer.getURL("/echo?status=200");
        ConnectivityDetector.overrideDefaultProbeUrlForTesting(hasContentUrl);
        String hasContentUrl2 = testServer.getURL("/echo?status=304");
        ConnectivityDetector.overrideFallbackProbeUrlForTesting(hasContentUrl2);

        setNetworkConnectivity(false);
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.DISCONNECTED, mConnectionState);
        setNetworkConnectivity(true);
        // Probing default URL gets back 200 which should trigger CAPTIVE_PORTAL state.
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.CAPTIVE_PORTAL, mConnectionState);
        // Twist the connection state to one different from CAPTIVE_PORTAL in order to get the
        // callback called with 2nd probe.
        setConnectionState(ConnectivityDetector.ConnectionState.NONE);
        // Probing fallback URL immediately after default URL gets back 304 should keep
        // CAPTIVE_PORTAL state.
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.CAPTIVE_PORTAL, mConnectionState);
    }

    @Test
    @MediumTest
    public void testDetectValidatedStateAfterBackoff() throws Exception {
        EmbeddedTestServer testServer =
                EmbeddedTestServer.createAndStartServer(InstrumentationRegistry.getContext());
        String hasContentUrl = testServer.getURL("/echo?status=200");
        String noContentUrl = testServer.getURL("/nocontent");
        ConnectivityDetector.overrideDefaultProbeUrlForTesting(hasContentUrl);
        ConnectivityDetector.overrideFallbackProbeUrlForTesting(hasContentUrl);

        setNetworkConnectivity(false);
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.DISCONNECTED, mConnectionState);
        setNetworkConnectivity(true);
        // Probing default URL gets back 200 which should trigger CAPTIVE_PORTAL state.
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.CAPTIVE_PORTAL, mConnectionState);
        // Twist the connection state to one different from CAPTIVE_PORTAL in order to get the
        // callback called with 2nd probe. This is because the callback will not be called if
        // connection state does not change based on the result from 2nd probe.
        setConnectionState(ConnectivityDetector.ConnectionState.NONE);
        // Probing fallback URL immediately after default URL gets back 200 which should trigger
        // CAPTIVE_PORTAL state.
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.CAPTIVE_PORTAL, mConnectionState);
        // After backoff, 2nd probe of default URL gets back "204 no content" which should trigger
        // VALIDATED state.
        ConnectivityDetector.overrideDefaultProbeUrlForTesting(noContentUrl);
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
        Assert.assertEquals(ConnectivityDetector.ConnectionState.VALIDATED, mConnectionState);
    }

    private static void setNetworkConnectivity(boolean connected) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { NetworkChangeNotifier.forceConnectivityState(connected); });
    }

    private boolean hasScheduledRetry() {
        final boolean[] result = new boolean[1];
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { result[0] = mConnectivityDetector.getHandlerForTesting().hasMessages(0); });
        return result[0];
    }

    private void checkConnectivityViaDefaultUrl() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mConnectivityDetector.setUseDefaultUrlForTesting(true);
            mConnectivityDetector.checkConnectivityViaHttpProbe();
        });
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    private void checkConnectivityViaFallbackUrl() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mConnectivityDetector.setUseDefaultUrlForTesting(false);
            mConnectivityDetector.checkConnectivityViaHttpProbe();
        });
        Assert.assertTrue(mSemaphore.tryAcquire(TIMEOUT_MS, TimeUnit.MILLISECONDS));
    }

    private void setConnectionState(@ConnectivityDetector.ConnectionState int connectionState) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mConnectivityDetector.forceConnectionStateForTesting(connectionState); });
    }
}
