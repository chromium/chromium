// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.url.GURL;

/** Java bridge to feed::SurfaceRenderer, provides shallow JNI bindings. */
@JNINamespace("feed::android")
public class FeedSurfaceRendererBridge {
    private int mSurfaceId;
    private long mNativeSurfaceRenderer;
    private Renderer mRenderer;

    /**
     * Calls from native to Java to implement rendering for the feed. See `feed::SurfaceRenderer`
     * for documentation.
     */
    public interface Renderer {
        void replaceDataStoreEntry(String key, byte[] data);

        void removeDataStoreEntry(String key);

        void onStreamUpdated(byte[] data);
    }

    /** Factory for FeedSurfaceRendererBridge. */
    public interface Factory {
        default FeedSurfaceRendererBridge create(
                Renderer renderer,
                FeedReliabilityLoggingBridge reliabilityLoggingBridge,
                @StreamKind int streamKind,
                SingleWebFeedParameters webFeedParameters) {
            return new FeedSurfaceRendererBridge(
                    renderer, reliabilityLoggingBridge, streamKind, webFeedParameters);
        }
    }

    public FeedSurfaceRendererBridge(
            Renderer renderer,
            FeedReliabilityLoggingBridge reliabilityLoggingBridge,
            @StreamKind int streamKind,
            SingleWebFeedParameters webFeedParameters) {
        mRenderer = renderer;
        if (streamKind == StreamKind.SINGLE_WEB_FEED) {
            mNativeSurfaceRenderer =
                    FeedSurfaceRendererBridgeJni.get()
                            .initWebFeed(
                                    this,
                                    webFeedParameters.getWebFeedId(),
                                    reliabilityLoggingBridge.getNativePtr(),
                                    webFeedParameters.getEntryPoint());
        } else {
            mNativeSurfaceRenderer =
                    FeedSurfaceRendererBridgeJni.get()
                            .init(this, streamKind, reliabilityLoggingBridge.getNativePtr());
        }
        mSurfaceId = FeedSurfaceRendererBridgeJni.get().getSurfaceId(mNativeSurfaceRenderer);
    }

    public int surfaceId() {
        return mSurfaceId;
    }

    public void destroy() {
        assert mRenderer != null;
        FeedSurfaceRendererBridgeJni.get().destroy(mNativeSurfaceRenderer);
        mNativeSurfaceRenderer = 0;
        mRenderer = null;
    }

    @CalledByNative
    private void replaceDataStoreEntry(String key, byte[] data) {
        if (mRenderer == null) {
            return;
        }
        mRenderer.replaceDataStoreEntry(key, data);
    }

    @CalledByNative
    private void removeDataStoreEntry(String key) {
        if (mRenderer == null) {
            return;
        }
        mRenderer.removeDataStoreEntry(key);
    }

    /** Called when the stream update content is available. The content will get passed to UI */
    @CalledByNative
    private void onStreamUpdated(byte[] data) {
        if (mRenderer == null) {
            return;
        }
        mRenderer.onStreamUpdated(data);
    }

    //
    // Methods bound to the surface instance, which do nothing after destroy().
    //

    void loadMore(Callback<Boolean> callback) {
        // Cancel if destroyed.
        if (mRenderer == null) {
            return;
        }
        FeedSurfaceRendererBridgeJni.get().loadMore(mNativeSurfaceRenderer, callback);
    }

    void manualRefresh(Callback<Boolean> callback) {
        // Cancel if destroyed.
        if (mRenderer == null) {
            return;
        }
        FeedSurfaceRendererBridgeJni.get().manualRefresh(mNativeSurfaceRenderer, callback);
    }

    void surfaceOpened() {
        // Cancel if destroyed.
        if (mRenderer == null) {
            return;
        }
        FeedSurfaceRendererBridgeJni.get().surfaceOpened(mNativeSurfaceRenderer);
    }

    void surfaceClosed() {
        // Cancel if destroyed.
        if (mRenderer == null) {
            return;
        }
        FeedSurfaceRendererBridgeJni.get().surfaceClosed(mNativeSurfaceRenderer);
    }

    //
    // Methods which may be called after destroy().
    //

    void reportFeedViewed() {
        FeedSurfaceRendererBridgeJni.get().reportFeedViewed(mSurfaceId);
    }

    void reportSliceViewed(String sliceId) {
        FeedSurfaceRendererBridgeJni.get().reportSliceViewed(mSurfaceId, sliceId);
    }

    void reportPageLoaded(boolean inNewTab) {
        FeedSurfaceRendererBridgeJni.get().reportPageLoaded(mSurfaceId, inNewTab);
    }

    void reportOpenAction(GURL url, String sliceId, @OpenActionType int openActionType) {
        FeedSurfaceRendererBridgeJni.get()
                .reportOpenAction(mSurfaceId, url, sliceId, openActionType);
    }

    void reportOpenVisitComplete(long visitTimeMs) {
        FeedSurfaceRendererBridgeJni.get().reportOpenVisitComplete(mSurfaceId, visitTimeMs);
    }

    void reportOtherUserAction(@FeedUserActionType int userAction) {
        FeedSurfaceRendererBridgeJni.get().reportOtherUserAction(mSurfaceId, userAction);
    }

    void reportStreamScrolled(int distanceDp) {
        FeedSurfaceRendererBridgeJni.get().reportStreamScrolled(mSurfaceId, distanceDp);
    }

    void reportStreamScrollStart() {
        FeedSurfaceRendererBridgeJni.get().reportStreamScrollStart(mSurfaceId);
    }

    void updateUserProfileOnLinkClick(GURL url, long[] mids) {
        FeedSurfaceRendererBridgeJni.get().updateUserProfileOnLinkClick(url, mids);
    }

    void processThereAndBackAgain(byte[] data, byte[] loggingParameters) {
        FeedSurfaceRendererBridgeJni.get().processThereAndBackAgain(data, loggingParameters);
    }

    int executeEphemeralChange(byte[] data) {
        return FeedSurfaceRendererBridgeJni.get().executeEphemeralChange(mSurfaceId, data);
    }

    void commitEphemeralChange(int changeId) {
        FeedSurfaceRendererBridgeJni.get().commitEphemeralChange(mSurfaceId, changeId);
    }

    void discardEphemeralChange(int changeId) {
        FeedSurfaceRendererBridgeJni.get().discardEphemeralChange(mSurfaceId, changeId);
    }

    long getLastFetchTimeMs() {
        return FeedSurfaceRendererBridgeJni.get().getLastFetchTimeMs(mSurfaceId);
    }

    void reportInfoCardTrackViewStarted(int type) {
        FeedSurfaceRendererBridgeJni.get().reportInfoCardTrackViewStarted(mSurfaceId, type);
    }

    void reportInfoCardViewed(int type, int minimumViewIntervalSeconds) {
        FeedSurfaceRendererBridgeJni.get()
                .reportInfoCardViewed(mSurfaceId, type, minimumViewIntervalSeconds);
    }

    void reportInfoCardClicked(int type) {
        FeedSurfaceRendererBridgeJni.get().reportInfoCardClicked(mSurfaceId, type);
    }

    void reportInfoCardDismissedExplicitly(int type) {
        FeedSurfaceRendererBridgeJni.get().reportInfoCardDismissedExplicitly(mSurfaceId, type);
    }

    void resetInfoCardStates(int type) {
        FeedSurfaceRendererBridgeJni.get().resetInfoCardStates(mSurfaceId, type);
    }

    void invalidateContentCacheFor(@StreamKind int feedToInvalidate) {
        FeedSurfaceRendererBridgeJni.get().invalidateContentCacheFor(feedToInvalidate);
    }

    void reportContentSliceVisibleTimeForGoodVisits(long elapsedMs) {
        FeedSurfaceRendererBridgeJni.get()
                .reportContentSliceVisibleTimeForGoodVisits(mSurfaceId, elapsedMs);
    }

    void contentViewed(long docid) {
        FeedSurfaceRendererBridgeJni.get().contentViewed(mSurfaceId, docid);
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        // Constructors.
        long init(
                FeedSurfaceRendererBridge caller,
                @StreamKind int streamKind,
                long nativeFeedReliabilityLoggingBridge);

        long initWebFeed(
                FeedSurfaceRendererBridge caller,
                byte[] webFeedId,
                long nativeFeedReliabilityLoggingBridge,
                int entryPoint);

        // Member functions, must not be called after destroy().
        void destroy(long nativeFeedSurfaceRendererBridge);

        void loadMore(long nativeFeedSurfaceRendererBridge, Callback<Boolean> callback);

        void manualRefresh(long nativeFeedSurfaceRendererBridge, Callback<Boolean> callback);

        int getSurfaceId(long nativeFeedSurfaceRendererBridge);

        void surfaceOpened(long nativeFeedSurfaceRendererBridge);

        void surfaceClosed(long nativeFeedSurfaceRendererBridge);

        // Static functions, can be called after creation, and destroy().
        void reportFeedViewed(int surfaceId);

        void reportSliceViewed(int surfaceId, String sliceId);

        void reportPageLoaded(int surfaceId, boolean inNewTab);

        void reportOpenAction(
                int surfaceId, GURL url, String sliceId, @OpenActionType int openActionType);

        void reportOpenVisitComplete(int surfaceId, long visitTimeMs);

        void reportOtherUserAction(int surfaceId, @FeedUserActionType int userAction);

        void reportStreamScrolled(int surfaceId, int distanceDp);

        void reportStreamScrollStart(int surfaceId);

        void updateUserProfileOnLinkClick(GURL url, long[] mids);

        void processThereAndBackAgain(byte[] data, byte[] loggingParameters);

        int executeEphemeralChange(int surfaceId, byte[] data);

        void commitEphemeralChange(int surfaceId, int changeId);

        void discardEphemeralChange(int surfaceId, int changeId);

        long getLastFetchTimeMs(int surfaceId);

        void reportInfoCardTrackViewStarted(int surfaceId, int type);

        void reportInfoCardViewed(int surfaceId, int type, int minimumViewIntervalSeconds);

        void reportInfoCardClicked(int surfaceId, int type);

        void reportInfoCardDismissedExplicitly(int surfaceId, int type);

        void resetInfoCardStates(int surfaceId, int type);

        void invalidateContentCacheFor(@StreamKind int feedToInvalidate);

        void reportContentSliceVisibleTimeForGoodVisits(int surfaceId, long elapsedMs);

        void contentViewed(int surfaceId, long docid);
    }
}
