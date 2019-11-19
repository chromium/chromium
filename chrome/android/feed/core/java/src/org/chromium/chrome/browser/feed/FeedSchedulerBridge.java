// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import androidx.annotation.NonNull;

import com.google.android.libraries.feed.api.client.requestmanager.RequestManager;
import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.feed.NativeRequestBehavior;

/**
 * Provides access to native implementations of SchedulerApi.
 */
@JNINamespace("feed")
public class FeedSchedulerBridge implements FeedScheduler {
    private long mNativeBridge;
    private RequestManager mRequestManager;

    /**
     * Creates a FeedSchedulerBridge for accessing native scheduling logic.
     *
     * @param profile Profile of the user we are rendering the Feed for.
     */
    public FeedSchedulerBridge(Profile profile) {
        mNativeBridge = FeedSchedulerBridgeJni.get().init(FeedSchedulerBridge.this, profile);
    }

    @Override
    public void destroy() {
        assert mNativeBridge != 0;
        FeedSchedulerBridgeJni.get().destroy(mNativeBridge, FeedSchedulerBridge.this);
        mNativeBridge = 0;
    }

    /**
     * Sets our copies for various interfaces provided by the Feed library. Should be done as early
     * as possible, as the scheduler will be unable to trigger refreshes until after it has the
     * mechanisms to correctly do so. When this is called, it is assumed that the given
     * RequestManager is initialized and can be used immediately.
     *
     * @param requestManager The interface that allows us make refresh requests.
     */
    public void initializeFeedDependencies(@NonNull RequestManager requestManager) {
        assert mRequestManager == null;
        mRequestManager = requestManager;
    }

    @Override
    public int shouldSessionRequestData(SessionState sessionState) {
        if (mNativeBridge == 0) return SchedulerApi.RequestBehavior.UNKNOWN;

        @NativeRequestBehavior
        int nativeBehavior = FeedSchedulerBridgeJni.get().shouldSessionRequestData(mNativeBridge,
                FeedSchedulerBridge.this, sessionState.hasContent,
                sessionState.contentCreationDateTimeMs, sessionState.hasOutstandingRequest);
        // If this breaks, it is because SchedulerApi.RequestBehavior and the NativeRequestBehavior
        // defined in feed_scheduler_host.h have diverged. If this happens during a feed DEPS roll,
        // it likely means that the native side needs to be updated. Note that this will not catch
        // new values and should handle value changes. Only removals/renames will cause compile
        // failures.
        switch (nativeBehavior) {
            case NativeRequestBehavior.REQUEST_WITH_WAIT:
                return SchedulerApi.RequestBehavior.REQUEST_WITH_WAIT;
            case NativeRequestBehavior.REQUEST_WITH_CONTENT:
                return SchedulerApi.RequestBehavior.REQUEST_WITH_CONTENT;
            case NativeRequestBehavior.REQUEST_WITH_TIMEOUT:
                return SchedulerApi.RequestBehavior.REQUEST_WITH_TIMEOUT;
            case NativeRequestBehavior.NO_REQUEST_WITH_WAIT:
                return SchedulerApi.RequestBehavior.NO_REQUEST_WITH_WAIT;
            case NativeRequestBehavior.NO_REQUEST_WITH_CONTENT:
                return SchedulerApi.RequestBehavior.NO_REQUEST_WITH_CONTENT;
            case NativeRequestBehavior.NO_REQUEST_WITH_TIMEOUT:
                return SchedulerApi.RequestBehavior.NO_REQUEST_WITH_TIMEOUT;
        }

        return SchedulerApi.RequestBehavior.UNKNOWN;
    }

    @Override
    public void onReceiveNewContent(long contentCreationDateTimeMs) {
        if (mNativeBridge != 0) {
            FeedSchedulerBridgeJni.get().onReceiveNewContent(
                    mNativeBridge, FeedSchedulerBridge.this, contentCreationDateTimeMs);
        }
    }

    @Override
    public void onRequestError(int networkResponseCode) {
        if (mNativeBridge != 0) {
            FeedSchedulerBridgeJni.get().onRequestError(
                    mNativeBridge, FeedSchedulerBridge.this, networkResponseCode);
        }
    }

    @Override
    public void onForegrounded() {
        assert mNativeBridge != 0;
        FeedSchedulerBridgeJni.get().onForegrounded(mNativeBridge, FeedSchedulerBridge.this);
    }

    @Override
    public void onFixedTimer(Runnable onCompletion) {
        assert mNativeBridge != 0;
        FeedSchedulerBridgeJni.get().onFixedTimer(
                mNativeBridge, FeedSchedulerBridge.this, onCompletion);
    }

    @Override
    public void onSuggestionConsumed() {
        assert mNativeBridge != 0;
        FeedSchedulerBridgeJni.get().onSuggestionConsumed(mNativeBridge, FeedSchedulerBridge.this);
    }

    @Override
    public boolean onArticlesCleared(boolean suppressRefreshes) {
        assert mNativeBridge != 0;
        return FeedSchedulerBridgeJni.get().onArticlesCleared(
                mNativeBridge, FeedSchedulerBridge.this, suppressRefreshes);
    }

    @CalledByNative
    private boolean triggerRefresh() {
        if (mRequestManager != null) {
            mRequestManager.triggerScheduledRefresh();
            return true;
        }
        return false;
    }

    @CalledByNative
    private void scheduleWakeUp(long thresholdMs) {
        FeedRefreshTask.scheduleWakeUp(thresholdMs);
    }

    @CalledByNative
    private void cancelWakeUp() {
        FeedRefreshTask.cancelWakeUp();
    }

    @NativeMethods
    interface Natives {
        long init(FeedSchedulerBridge caller, Profile profile);
        void destroy(long nativeFeedSchedulerBridge, FeedSchedulerBridge caller);
        int shouldSessionRequestData(long nativeFeedSchedulerBridge, FeedSchedulerBridge caller,
                boolean hasContent, long contentCreationDateTimeMs, boolean hasOutstandingRequest);
        void onReceiveNewContent(long nativeFeedSchedulerBridge, FeedSchedulerBridge caller,
                long contentCreationDateTimeMs);
        void onRequestError(long nativeFeedSchedulerBridge, FeedSchedulerBridge caller,
                int networkResponseCode);
        void onForegrounded(long nativeFeedSchedulerBridge, FeedSchedulerBridge caller);
        void onFixedTimer(
                long nativeFeedSchedulerBridge, FeedSchedulerBridge caller, Runnable onCompletion);
        void onSuggestionConsumed(long nativeFeedSchedulerBridge, FeedSchedulerBridge caller);
        boolean onArticlesCleared(long nativeFeedSchedulerBridge, FeedSchedulerBridge caller,
                boolean suppressRefreshes);
    }
}
