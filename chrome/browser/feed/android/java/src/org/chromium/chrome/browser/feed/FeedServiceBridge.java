// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.Context;
import android.util.DisplayMetrics;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeClassQualifiedName;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.feed.v2.ContentOrder;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.chrome.browser.xsurface.ImageCacheHelper;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface_provider.XSurfaceProcessScopeProvider;

import java.util.Locale;

/** Bridge for FeedService-related calls. */
@JNINamespace("feed")
public final class FeedServiceBridge {
    // Access to JNI test hooks for other libraries. This can go away once more Feed code is
    // migrated to chrome/browser/feed.
    public static org.jni_zero.JniStaticTestMocker<FeedServiceBridge.Natives>
            getTestHooksForTesting() {
        return FeedServiceBridgeJni.TEST_HOOKS;
    }

    public static ProcessScope xSurfaceProcessScope() {
        return XSurfaceProcessScopeProvider.getProcessScope();
    }

    public static boolean isEnabled() {
        return FeedServiceBridgeJni.get().isEnabled();
    }

    /** Returns the top user specified locale. */
    private static Locale getLocale(Context context) {
        return context.getResources().getConfiguration().getLocales().get(0);
    }

    // Java functionality needed for the native FeedService.
    @CalledByNative
    public static String getLanguageTag() {
        return getLocale(ContextUtils.getApplicationContext()).toLanguageTag();
    }

    @CalledByNative
    public static double[] getDisplayMetrics() {
        DisplayMetrics metrics =
                ContextUtils.getApplicationContext().getResources().getDisplayMetrics();
        double[] result = {metrics.density, metrics.widthPixels, metrics.heightPixels};
        return result;
    }

    @CalledByNative
    public static void clearAll() {
        FeedSurfaceTracker.getInstance().clearAll();
    }

    @CalledByNative
    public static void prefetchImage(String url) {
        ProcessScope processScope = xSurfaceProcessScope();
        if (processScope != null) {
            ImageCacheHelper imageCacheHelper = processScope.provideImageCacheHelper();
            if (imageCacheHelper != null) {
                imageCacheHelper.prefetchImage(url);
            }
        }
    }

    /** Called at startup to trigger creation of |FeedService|. */
    public static void startup() {
        FeedServiceBridgeJni.get().startup();
    }

    /** Retrieves the config value for load_more_trigger_lookahead. */
    public static int getLoadMoreTriggerLookahead() {
        return FeedServiceBridgeJni.get().getLoadMoreTriggerLookahead();
    }

    /** Retrieves the config value for load_more_trigger_scroll_distance_dp. */
    public static int getLoadMoreTriggerScrollDistanceDp() {
        return FeedServiceBridgeJni.get().getLoadMoreTriggerScrollDistanceDp();
    }

    public static long getReliabilityLoggingId() {
        return FeedServiceBridgeJni.get().getReliabilityLoggingId();
    }

    public static @ContentOrder int getContentOrderForWebFeed() {
        return FeedServiceBridgeJni.get().getContentOrderForWebFeed();
    }

    public static void setContentOrderForWebFeed(@ContentOrder int contentOrder) {
        FeedServiceBridgeJni.get().setContentOrderForWebFeed(contentOrder);
    }

    /**
     * Reports that a user action occurred which is untied to a Feed tab. Use
     * FeedStream.reportOtherUserAction for stream-specific actions.
     */
    public static void reportOtherUserAction(
            @StreamKind int streamKind, @FeedUserActionType int userAction) {
        FeedServiceBridgeJni.get().reportOtherUserAction(streamKind, userAction);
    }

    /**
     * @return True if the user is signed in for feed purposes (i.e. if a personalized feed can be
     *         requested).
     */
    public static boolean isSignedIn() {
        return FeedServiceBridgeJni.get().isSignedIn();
    }

    /** Observes whether or not the Feed stream contains unread content */
    public static class UnreadContentObserver {
        private long mNativePtr;

        /**
         * Begins observing.
         *
         * @param isWebFeed  Whether to observe the Web Feed, or the For-you Feed.
         */
        public UnreadContentObserver(boolean isWebFeed) {
            mNativePtr = FeedServiceBridgeJni.get().addUnreadContentObserver(this, isWebFeed);
        }

        /** Stops observing. Must be called when this observer is no longer needed */
        public void destroy() {
            FeedServiceBridgeJni.get().destroy(mNativePtr);
            mNativePtr = 0;
        }

        /**
         * Called to signal whether unread content is available. Called once after the observer is
         * initialized, and after that, called each time unread content status changes.
         */
        @CalledByNative("UnreadContentObserver")
        public void hasUnreadContentChanged(boolean hasUnreadContent) {}
    }

    @NativeMethods
    public interface Natives {
        boolean isEnabled();

        void startup();

        int getLoadMoreTriggerLookahead();

        int getLoadMoreTriggerScrollDistanceDp();

        long getReliabilityLoggingId();

        void reportOtherUserAction(@StreamKind int streamKind, @FeedUserActionType int userAction);

        @ContentOrder
        int getContentOrderForWebFeed();

        void setContentOrderForWebFeed(@ContentOrder int contentOrder);

        long addUnreadContentObserver(Object object, boolean isWebFeed);

        boolean isSignedIn();

        @NativeClassQualifiedName("feed::JavaUnreadContentObserver")
        void destroy(long nativePtr);
    }
}
