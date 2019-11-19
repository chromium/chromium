// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.net.nqe;

import static org.junit.Assert.assertEquals;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.net.EffectiveConnectionType;

/**
 * JUnit tests for NetworkQualityProvider which run against Robolectric.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class NetworkQualityProviderTest {
    private static final long HTTP_RTT = 1;
    private static final long TRANSPORT_RTT = 2;
    private static final int KBPS = 3;
    private static final @EffectiveConnectionType int CONNECTION_TYPE = 4;

    private NetworkQualityProvider mProvider;
    private TestNetworkQualityObserver mObserver;

    private static class NetworkQualityProviderForTesting extends NetworkQualityProvider {
        @Override
        protected void doNativeInit() {}

        public static void reset(NetworkQualityProvider provider) {
            sInstance = provider;
        }
    }

    private static class TestNetworkQualityObserver implements NetworkQualityObserver {
        private int mConnectionTypeCount;
        private int mRTTCount;

        public int getConnectionTypeCount() {
            return mConnectionTypeCount;
        }

        public int getRTTCount() {
            return mRTTCount;
        }

        @Override
        public void onEffectiveConnectionTypeChanged(
                @EffectiveConnectionType int effectiveConnectionType) {
            ++mConnectionTypeCount;
            assertEquals(CONNECTION_TYPE, effectiveConnectionType);
        }

        @Override
        public void onRTTOrThroughputEstimatesComputed(
                long httpRTTMillis, long transportRTTMillis, int downstreamThroughputKbps) {
            ++mRTTCount;
            assertEquals(HTTP_RTT, httpRTTMillis);
            assertEquals(TRANSPORT_RTT, transportRTTMillis);
            assertEquals(KBPS, downstreamThroughputKbps);
        }
    }

    @Before
    public void setUp() {
        mProvider = new NetworkQualityProviderForTesting();
        NetworkQualityProviderForTesting.reset(mProvider);
        mObserver = new TestNetworkQualityObserver();
    }

    @Test
    public void testObserversGetNotified() {
        NetworkQualityProvider.addObserverAndMaybeTrigger(mObserver);

        assertEquals(0, mObserver.getConnectionTypeCount());
        assertEquals(0, mObserver.getRTTCount());

        mProvider.onEffectiveConnectionTypeChanged(CONNECTION_TYPE);
        assertEquals(1, mObserver.getConnectionTypeCount());
        assertEquals(0, mObserver.getRTTCount());

        mProvider.onRTTOrThroughputEstimatesComputed(HTTP_RTT, TRANSPORT_RTT, KBPS);
        assertEquals(1, mObserver.getConnectionTypeCount());
        assertEquals(1, mObserver.getRTTCount());
    }

    @Test
    public void testRemovedObserversDontGetNotified() {
        NetworkQualityProvider.addObserverAndMaybeTrigger(mObserver);
        NetworkQualityProvider.removeObserver(mObserver);

        mProvider.onEffectiveConnectionTypeChanged(CONNECTION_TYPE);
        mProvider.onRTTOrThroughputEstimatesComputed(HTTP_RTT, TRANSPORT_RTT, KBPS);
        assertEquals(0, mObserver.getConnectionTypeCount());
        assertEquals(0, mObserver.getRTTCount());
    }

    @Test
    public void testObserversGetNotifiedWhenAdded() {
        mProvider.onEffectiveConnectionTypeChanged(CONNECTION_TYPE);
        NetworkQualityProvider.addObserverAndMaybeTrigger(mObserver);
        assertEquals(1, mObserver.getConnectionTypeCount());
        assertEquals(0, mObserver.getRTTCount());

        NetworkQualityProvider.removeObserver(mObserver);
        mProvider.onRTTOrThroughputEstimatesComputed(HTTP_RTT, TRANSPORT_RTT, KBPS);
        NetworkQualityProvider.addObserverAndMaybeTrigger(mObserver);
        assertEquals(2, mObserver.getConnectionTypeCount());
        assertEquals(1, mObserver.getRTTCount());
        NetworkQualityProvider.removeObserver(mObserver);

        mProvider = new NetworkQualityProviderForTesting();
        NetworkQualityProviderForTesting.reset(mProvider);
        mProvider.onRTTOrThroughputEstimatesComputed(HTTP_RTT, TRANSPORT_RTT, KBPS);
        NetworkQualityProvider.addObserverAndMaybeTrigger(mObserver);
        assertEquals(2, mObserver.getConnectionTypeCount());
        assertEquals(2, mObserver.getRTTCount());
    }
}