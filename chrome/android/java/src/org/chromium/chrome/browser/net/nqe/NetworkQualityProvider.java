// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.net.nqe;

import org.chromium.base.ObserverList;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.net.EffectiveConnectionType;

/**
 * Provides Network Quality Estimates to observers.
 */
public class NetworkQualityProvider {
    protected static NetworkQualityProvider sInstance;

    private final ObserverList<NetworkQualityObserver> mObservers = new ObserverList<>();

    private final RewindableIterator<NetworkQualityObserver> mRewindableIterator;

    private boolean mInitializedRtt;
    private long mHttpRttMillis;
    private long mTransportRttMillis;
    private int mDownstreamThroughputKbps;
    private @EffectiveConnectionType Integer mEffectiveConnectionType;

    private long mNativeNetworkQualityProvider;

    /**
     * @param observer The {@link NetworkQualityObserver} to be called when connection changes
     *            occur. This will trigger all observer callbacks for which data is already
     *            available.
     */
    public static void addObserverAndMaybeTrigger(NetworkQualityObserver observer) {
        NetworkQualityProvider provider = getInstance();
        provider.mObservers.addObserver(observer);
        if (provider.mEffectiveConnectionType != null) {
            observer.onEffectiveConnectionTypeChanged(provider.mEffectiveConnectionType);
        }
        if (provider.mInitializedRtt) {
            observer.onRTTOrThroughputEstimatesComputed(provider.mHttpRttMillis,
                    provider.mTransportRttMillis, provider.mDownstreamThroughputKbps);
        }
    }

    /**
     * @param observer The {@link NetworkQualityObserver} to remove.
     */
    public static void removeObserver(NetworkQualityObserver observer) {
        NetworkQualityProvider provider = getInstance();
        provider.mObservers.removeObserver(observer);
    }

    private static NetworkQualityProvider getInstance() {
        if (sInstance == null) sInstance = new NetworkQualityProvider();
        return sInstance;
    }

    // Protected for testing.
    protected NetworkQualityProvider() {
        sInstance = this;
        mRewindableIterator = mObservers.rewindableIterator();
        doNativeInit();
    }

    protected void doNativeInit() {
        assert BrowserStartupController.getInstance().isFullBrowserStarted();
        mNativeNetworkQualityProvider =
                NetworkQualityProviderJni.get().init(NetworkQualityProvider.this);
    }

    @CalledByNative
    public void onEffectiveConnectionTypeChanged(
            @EffectiveConnectionType int effectiveConnectionType) {
        mEffectiveConnectionType = effectiveConnectionType;
        mRewindableIterator.rewind();
        while (mRewindableIterator.hasNext()) {
            mRewindableIterator.next().onEffectiveConnectionTypeChanged(mEffectiveConnectionType);
        }
    }

    @CalledByNative
    public void onRTTOrThroughputEstimatesComputed(
            long httpRTTMillis, long transportRTTMillis, int downstreamThroughputKbps) {
        mHttpRttMillis = httpRTTMillis;
        mTransportRttMillis = transportRTTMillis;
        mDownstreamThroughputKbps = downstreamThroughputKbps;
        mInitializedRtt = true;
        mRewindableIterator.rewind();
        while (mRewindableIterator.hasNext()) {
            mRewindableIterator.next().onRTTOrThroughputEstimatesComputed(
                    mHttpRttMillis, mTransportRttMillis, mDownstreamThroughputKbps);
        }
    }

    @NativeMethods
    interface Natives {
        long init(NetworkQualityProvider caller);
    }
}
