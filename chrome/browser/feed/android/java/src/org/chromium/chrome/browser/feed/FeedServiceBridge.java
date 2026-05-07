// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.content.Context;
import android.util.DisplayMetrics;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feed.v2.FeedUserActionType;
import org.chromium.chrome.browser.xsurface.ImageCacheHelper;
import org.chromium.chrome.browser.xsurface.ProcessScope;
import org.chromium.chrome.browser.xsurface_provider.XSurfaceProcessScopeProvider;

import java.util.Locale;

/** Bridge for FeedService-related calls. */
@JNINamespace("feed")
@NullMarked
public final class FeedServiceBridge {
    public static @Nullable ProcessScope xSurfaceProcessScope() {
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
    public static @JniType("std::string") String getLanguageTag() {
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
    public static void prefetchImage(@JniType("std::string") String url) {
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

    /** Reports that a user action occurred which is associated with a feed stream. */
    public static void reportOtherUserAction(
            @StreamKind int streamKind, @FeedUserActionType int userAction) {
        FeedServiceBridgeJni.get().reportOtherUserActionForStream(streamKind, userAction);
    }

    /** Reports that a user action occurred which is independent of any feed stream. */
    public static void reportOtherUserAction(@FeedUserActionType int userAction) {
        FeedServiceBridgeJni.get().reportOtherUserAction(userAction);
    }

    /**
     * @return True if the user is signed in for feed purposes (i.e. if a personalized feed can be
     *     requested).
     */
    public static boolean isSignedIn() {
        return FeedServiceBridgeJni.get().isSignedIn();
    }

    @NativeMethods
    public interface Natives {
        boolean isEnabled();

        void startup();

        int getLoadMoreTriggerLookahead();

        int getLoadMoreTriggerScrollDistanceDp();

        long getReliabilityLoggingId();

        void reportOtherUserActionForStream(
                @StreamKind int streamKind, @FeedUserActionType int userAction);

        void reportOtherUserAction(@FeedUserActionType int userAction);

        boolean isSignedIn();
    }
}
