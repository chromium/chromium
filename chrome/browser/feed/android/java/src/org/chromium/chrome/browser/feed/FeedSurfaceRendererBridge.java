// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

/** Java bridge to feed::SurfaceRenderer, provides shallow JNI bindings. */
@JNINamespace("feed::android")
@NullMarked
public class FeedSurfaceRendererBridge {
    private final Profile mProfile;
    private final int mSurfaceId;
    private long mNativeSurfaceRenderer;
    private @Nullable Renderer mRenderer;

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
                Profile profile,
                Renderer renderer,
                FeedReliabilityLoggingBridge reliabilityLoggingBridge,
                @StreamKind int streamKind,
                SingleWebFeedParameters webFeedParameters) {
            return new FeedSurfaceRendererBridge(
                    profile, renderer, reliabilityLoggingBridge, streamKind, webFeedParameters);
        }
    }

    public FeedSurfaceRendererBridge(
            Profile profile,
            Renderer renderer,
            FeedReliabilityLoggingBridge reliabilityLoggingBridge,
            @StreamKind int streamKind,
            SingleWebFeedParameters webFeedParameters) {
        mProfile = profile;
        mRenderer = renderer;
        if (streamKind == StreamKind.SINGLE_WEB_FEED) {
            mNativeSurfaceRenderer =
                    FeedSurfaceRendererBridgeJni.get()
                            .initWebFeed(
                                    this,
                                    profile,
                                    webFeedParameters.getWebFeedId(),
                                    reliabilityLoggingBridge.getNativePtr(),
                                    webFeedParameters.getEntryPoint());
        } else {
            mNativeSurfaceRenderer =
                    FeedSurfaceRendererBridgeJni.get()
                            .init(
                                    this,
                                    profile,
                                    streamKind,
                                    reliabilityLoggingBridge.getNativePtr());
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
        FeedSurfaceRendererBridgeJni.get().reportFeedViewed(mProfile, mSurfaceId);
    }

    void reportSliceViewed(String sliceId) {
        FeedSurfaceRendererBridgeJni.get().reportSliceViewed(mProfile, mSurfaceId, sliceId);
    }

    void reportPageLoaded(boolean inNewTab) {
        FeedSurfaceRendererBridgeJni.get().reportPageLoaded(mProfile, mSurfaceId, inNewTab);
    }

    void reportOpenAction(GURL url, @Nullable String sliceId, @OpenActionType int openActionType) {
        FeedSurfaceRendererBridgeJni.get()
                .reportOpenAction(mProfile, mSurfaceId, url, sliceId, openActionType);
    }

    void reportOpenVisitComplete(long visitTimeMs) {
        FeedSurfaceRendererBridgeJni.get()
                .reportOpenVisitComplete(mProfile, mSurfaceId, visitTimeMs);
    }

    void reportOtherUserAction(@FeedUserActionType int userAction) {
        FeedSurfaceRendererBridgeJni.get().reportOtherUserAction(mProfile, mSurfaceId, userAction);
    }

    void reportStreamScrolled(int distanceDp) {
        FeedSurfaceRendererBridgeJni.get().reportStreamScrolled(mProfile, mSurfaceId, distanceDp);
    }

    void reportStreamScrollStart() {
        FeedSurfaceRendererBridgeJni.get().reportStreamScrollStart(mProfile, mSurfaceId);
    }

    void updateUserProfileOnLinkClick(GURL url, long[] mids) {
        FeedSurfaceRendererBridgeJni.get().updateUserProfileOnLinkClick(mProfile, url, mids);
    }

    void processThereAndBackAgain(byte[] data, byte[] loggingParameters) {
        FeedSurfaceRendererBridgeJni.get()
                .processThereAndBackAgain(mProfile, data, loggingParameters);
    }

    int executeEphemeralChange(byte[] data) {
        return FeedSurfaceRendererBridgeJni.get()
                .executeEphemeralChange(mProfile, mSurfaceId, data);
    }

    void commitEphemeralChange(int changeId) {
        FeedSurfaceRendererBridgeJni.get().commitEphemeralChange(mProfile, mSurfaceId, changeId);
    }

    void discardEphemeralChange(int changeId) {
        FeedSurfaceRendererBridgeJni.get().discardEphemeralChange(mProfile, mSurfaceId, changeId);
    }

    long getLastFetchTimeMs() {
        return FeedSurfaceRendererBridgeJni.get().getLastFetchTimeMs(mProfile, mSurfaceId);
    }

    void reportInfoCardTrackViewStarted(int type) {
        FeedSurfaceRendererBridgeJni.get()
                .reportInfoCardTrackViewStarted(mProfile, mSurfaceId, type);
    }

    void reportInfoCardViewed(int type, int minimumViewIntervalSeconds) {
        FeedSurfaceRendererBridgeJni.get()
                .reportInfoCardViewed(mProfile, mSurfaceId, type, minimumViewIntervalSeconds);
    }

    void reportInfoCardClicked(int type) {
        FeedSurfaceRendererBridgeJni.get().reportInfoCardClicked(mProfile, mSurfaceId, type);
    }

    void reportInfoCardDismissedExplicitly(int type) {
        FeedSurfaceRendererBridgeJni.get()
                .reportInfoCardDismissedExplicitly(mProfile, mSurfaceId, type);
    }

    void resetInfoCardStates(int type) {
        FeedSurfaceRendererBridgeJni.get().resetInfoCardStates(mProfile, mSurfaceId, type);
    }

    void invalidateContentCacheFor(@StreamKind int feedToInvalidate) {
        FeedSurfaceRendererBridgeJni.get().invalidateContentCacheFor(mProfile, feedToInvalidate);
    }

    void reportContentSliceVisibleTimeForGoodVisits(long elapsedMs) {
        FeedSurfaceRendererBridgeJni.get()
                .reportContentSliceVisibleTimeForGoodVisits(mProfile, mSurfaceId, elapsedMs);
    }

    void contentViewed(long docid) {
        FeedSurfaceRendererBridgeJni.get().contentViewed(mProfile, mSurfaceId, docid);
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        // Constructors.
        long init(
                FeedSurfaceRendererBridge caller,
                @JniType("Profile*") Profile profile,
                @StreamKind int streamKind,
                long nativeFeedReliabilityLoggingBridge);

        long initWebFeed(
                FeedSurfaceRendererBridge caller,
                @JniType("Profile*") Profile profile,
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
        void reportFeedViewed(@JniType("Profile*") Profile profile, int surfaceId);

        void reportSliceViewed(
                @JniType("Profile*") Profile profile,
                int surfaceId,
                @JniType("std::string") String sliceId);

        void reportPageLoaded(
                @JniType("Profile*") Profile profile, int surfaceId, boolean inNewTab);

        void reportOpenAction(
                @JniType("Profile*") Profile profile,
                int surfaceId,
                GURL url,
                @JniType("std::string") @Nullable String sliceId,
                @OpenActionType int openActionType);

        void reportOpenVisitComplete(
                @JniType("Profile*") Profile profile, int surfaceId, long visitTimeMs);

        void reportOtherUserAction(
                @JniType("Profile*") Profile profile,
                int surfaceId,
                @FeedUserActionType int userAction);

        void reportStreamScrolled(
                @JniType("Profile*") Profile profile, int surfaceId, int distanceDp);

        void reportStreamScrollStart(@JniType("Profile*") Profile profile, int surfaceId);

        void updateUserProfileOnLinkClick(
                @JniType("Profile*") Profile profile, GURL url, long[] mids);

        void processThereAndBackAgain(
                @JniType("Profile*") Profile profile, byte[] data, byte[] loggingParameters);

        int executeEphemeralChange(
                @JniType("Profile*") Profile profile, int surfaceId, byte[] data);

        void commitEphemeralChange(
                @JniType("Profile*") Profile profile, int surfaceId, int changeId);

        void discardEphemeralChange(
                @JniType("Profile*") Profile profile, int surfaceId, int changeId);

        long getLastFetchTimeMs(@JniType("Profile*") Profile profile, int surfaceId);

        void reportInfoCardTrackViewStarted(
                @JniType("Profile*") Profile profile, int surfaceId, int type);

        void reportInfoCardViewed(
                @JniType("Profile*") Profile profile,
                int surfaceId,
                int type,
                int minimumViewIntervalSeconds);

        void reportInfoCardClicked(@JniType("Profile*") Profile profile, int surfaceId, int type);

        void reportInfoCardDismissedExplicitly(
                @JniType("Profile*") Profile profile, int surfaceId, int type);

        void resetInfoCardStates(@JniType("Profile*") Profile profile, int surfaceId, int type);

        void invalidateContentCacheFor(
                @JniType("Profile*") Profile profile, @StreamKind int feedToInvalidate);

        void reportContentSliceVisibleTimeForGoodVisits(
                @JniType("Profile*") Profile profile, int surfaceId, long elapsedMs);

        void contentViewed(@JniType("Profile*") Profile profile, int surfaceId, long docid);
    }
}
