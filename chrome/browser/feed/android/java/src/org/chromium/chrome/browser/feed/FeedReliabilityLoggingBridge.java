// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.xsurface.FeedLaunchReliabilityLogger;
import org.chromium.chrome.browser.xsurface.FeedNetworkRequestReliabilityLogger;

/** JNI bridge making reliability logging methods available to native code. */
@JNINamespace("feed::android")
public class FeedReliabilityLoggingBridge {
    private final long mNativePtr;
    private FeedLaunchReliabilityLogger mLogger;

    public static org.chromium.base.JniStaticTestMocker<FeedReliabilityLoggingBridge.Natives>
    getTestHooksForTesting() {
        return FeedReliabilityLoggingBridgeJni.TEST_HOOKS;
    }

    public FeedReliabilityLoggingBridge() {
        // mLogger should be null until FeedStream.bind() calls setLogger(). We don't expect mLogger
        // to be used until then.
        mNativePtr = FeedReliabilityLoggingBridgeJni.get().init(this);
    }

    public void setLogger(FeedLaunchReliabilityLogger logger) {
        mLogger = logger;
    }

    public long getNativePtr() {
        return mNativePtr;
    }

    public void destroy() {
        FeedReliabilityLoggingBridgeJni.get().destroy(mNativePtr);
    }

    @CalledByNative
    public void sendPendingEvents(int streamType, int streamId) {
        mLogger.sendPendingEvents(streamType, streamId);
    }

    @CalledByNative
    public void cancelPendingEvents() {
        mLogger.cancelPendingEvents();
    }

    @CalledByNative
    public void logOtherLaunchStart(long timestamp) {
        mLogger.logFeedLaunchOtherStart(timestamp);
    }

    @CalledByNative
    public void logCacheReadStart(long timestamp) {
        mLogger.logCacheReadStart(timestamp);
    }

    @CalledByNative
    public void logCacheReadEnd(long timestamp, int cacheReadResult) {
        mLogger.logCacheReadEnd(timestamp, cacheReadResult);
    }

    @CalledByNative
    public int logFeedRequestStart(long timestamp) {
        return mLogger.logFeedQueryRequestStart(timestamp);
    }

    @CalledByNative
    public int logActionsUploadRequestStart(long timestamp) {
        return mLogger.logActionsUploadRequestStart(timestamp);
    }

    @CalledByNative
    public void logRequestSent(int requestId, long timestamp) {
        FeedNetworkRequestReliabilityLogger requestLogger =
                mLogger.getNetworkRequestReliabilityLogger(requestId);
        if (requestLogger != null) {
            requestLogger.logRequestSent(timestamp);
        }
    }

    @CalledByNative
    public void logResponseReceived(int requestId, long serverRecvTimestamp,
            long serverSendTimestamp, long clientRecvTimestamp) {
        FeedNetworkRequestReliabilityLogger requestLogger =
                mLogger.getNetworkRequestReliabilityLogger(requestId);
        if (requestLogger != null) {
            requestLogger.logResponseReceived(
                    serverRecvTimestamp, serverSendTimestamp, clientRecvTimestamp);
        }
    }

    @CalledByNative
    public void logRequestFinished(int requestId, long timestamp, int canonicalStatus) {
        FeedNetworkRequestReliabilityLogger requestLogger =
                mLogger.getNetworkRequestReliabilityLogger(requestId);
        if (requestLogger != null) {
            requestLogger.logRequestFinished(timestamp, canonicalStatus);
        }
    }

    @CalledByNative
    public void logAtfRenderStart(long timestamp) {
        mLogger.logAtfRenderStart(timestamp);
    }

    @CalledByNative
    public void logAtfRenderEnd(long timestamp, int aboveTheFoldRenderResult) {
        mLogger.logAtfRenderEnd(timestamp, aboveTheFoldRenderResult);
    }

    @CalledByNative
    public void logLaunchFinished(long timestamp, int discoverLaunchResult) {
        mLogger.logLaunchFinished(timestamp, discoverLaunchResult);
    }

    @NativeMethods
    public interface Natives {
        long init(FeedReliabilityLoggingBridge thisRef);
        void destroy(long nativeFeedReliabilityLoggingBridge);
    }
}