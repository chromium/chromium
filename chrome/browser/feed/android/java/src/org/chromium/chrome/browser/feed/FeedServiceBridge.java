// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.annotation.TargetApi;
import android.content.Context;
import android.os.Build;
import android.util.DisplayMetrics;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.xsurface.ImagePrefetcher;
import org.chromium.chrome.browser.xsurface.ProcessScope;

import java.util.Locale;

/**
 * Bridge for FeedService-related calls.
 */
@JNINamespace("feed")
public final class FeedServiceBridge {
    // Access to JNI test hooks for other libraries. This can go away once more Feed code is
    // migrated to chrome/browser/feed.
    public static org.chromium.base.JniStaticTestMocker<FeedServiceBridge.Natives>
    getTestHooksForTesting() {
        return FeedServiceBridgeJni.TEST_HOOKS;
    }

    /**
     * Interface to chrome_java. Eventually, we will move some of these pieces into the Feed module
     * to eliminate the need for an interface here.
     */
    public static interface Delegate {
        default ProcessScope getProcessScope() {
            return null;
        }
        /** Called when state of the feed must be cleared. */
        default void clearAll() {}
    }

    private static Delegate sDelegate = new Delegate() {};
    public static void setDelegate(Delegate delegate) {
        sDelegate = delegate;
    }

    public static ProcessScope xSurfaceProcessScope() {
        return sDelegate.getProcessScope();
    }
    public static boolean isEnabled() {
        return FeedServiceBridgeJni.get().isEnabled();
    }

    /** Returns the top user specified locale. */
    @TargetApi(Build.VERSION_CODES.N)
    private static Locale getLocale(Context context) {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.N
                ? context.getResources().getConfiguration().getLocales().get(0)
                : context.getResources().getConfiguration().locale;
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
        sDelegate.clearAll();
    }

    @CalledByNative
    public static void prefetchImage(String url) {
        ProcessScope processScope = xSurfaceProcessScope();
        if (processScope != null) {
            ImagePrefetcher imagePrefetcher = processScope.provideImagePrefetcher();
            if (imagePrefetcher != null) {
                imagePrefetcher.prefetchImage(url);
            }
        }
    }

    /** Called at startup to trigger creation of |FeedService|. */
    public static void startup() {
        FeedServiceBridgeJni.get().startup();
    }

    public static String getClientInstanceId() {
        return FeedServiceBridgeJni.get().getClientInstanceId();
    }

    /** Retrieves the config value for load_more_trigger_lookahead. */
    public static int getLoadMoreTriggerLookahead() {
        return FeedServiceBridgeJni.get().getLoadMoreTriggerLookahead();
    }

    /** Retrieves the config value for load_more_trigger_scroll_distance_dp. */
    public static int getLoadMoreTriggerScrollDistanceDp() {
        return FeedServiceBridgeJni.get().getLoadMoreTriggerScrollDistanceDp();
    }

    public static void reportOpenVisitComplete(long visitTimeMs) {
        FeedServiceBridgeJni.get().reportOpenVisitComplete(visitTimeMs);
    }

    public static @VideoPreviewsType int getVideoPreviewsTypePreference() {
        return FeedServiceBridgeJni.get().getVideoPreviewsTypePreference();
    }

    public static void setVideoPreviewsTypePreference(@VideoPreviewsType int videoPreviewsType) {
        FeedServiceBridgeJni.get().setVideoPreviewsTypePreference(videoPreviewsType);
    }

    @NativeMethods
    public interface Natives {
        boolean isEnabled();
        void startup();
        int getLoadMoreTriggerLookahead();
        int getLoadMoreTriggerScrollDistanceDp();
        String getClientInstanceId();
        void reportOpenVisitComplete(long visitTimeMs);
        int getVideoPreviewsTypePreference();
        void setVideoPreviewsTypePreference(int videoPreviewsType);
    }
}
