// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.v2;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.BundleUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.GoogleAPIKeys;
import org.chromium.chrome.browser.base.SplitCompatUtils;
import org.chromium.chrome.browser.feed.FeedImageFetchClient;
import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.xsurface.ImageFetchClient;
import org.chromium.chrome.browser.xsurface.PersistentKeyValueCache;
import org.chromium.chrome.browser.xsurface.ProcessScopeDependencyProvider;
import org.chromium.components.version_info.VersionConstants;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * Provides logging and context for all surfaces.
 */
public class FeedProcessScopeDependencyProvider implements ProcessScopeDependencyProvider {
    private static final String FEED_SPLIT_NAME = "feedv2";

    private Context mContext;
    private ImageFetchClient mImageFetchClient;
    private FeedPersistentKeyValueCache mPersistentKeyValueCache;
    private LibraryResolver mLibraryResolver;

    @VisibleForTesting
    static PrivacyPreferencesManager sPrivacyPreferencesManagerForTest;

    public FeedProcessScopeDependencyProvider() {
        mContext = createFeedContext(ContextUtils.getApplicationContext());
        mImageFetchClient = new FeedImageFetchClient();
        mPersistentKeyValueCache = new FeedPersistentKeyValueCache();
        if (BundleUtils.isIsolatedSplitInstalled(mContext, FEED_SPLIT_NAME)) {
            mLibraryResolver = (libName) -> {
                return BundleUtils.getNativeLibraryPath(libName, FEED_SPLIT_NAME);
            };
        }
    }

    @Override
    public Context getContext() {
        return mContext;
    }

    @Override
    public ImageFetchClient getImageFetchClient() {
        return mImageFetchClient;
    }

    @Override
    public PersistentKeyValueCache getPersistentKeyValueCache() {
        return mPersistentKeyValueCache;
    }

    @Override
    public void logError(String tag, String format, Object... args) {
        Log.e(tag, format, args);
    }

    @Override
    public void logWarning(String tag, String format, Object... args) {
        Log.w(tag, format, args);
    }

    @Override
    public void postTask(int taskType, Runnable task, long delayMs) {
        TaskTraits traits;
        switch (taskType) {
            case ProcessScopeDependencyProvider.TASK_TYPE_UI_THREAD:
                traits = UiThreadTaskTraits.DEFAULT;
                break;
            case ProcessScopeDependencyProvider.TASK_TYPE_BACKGROUND_MAY_BLOCK:
                traits = TaskTraits.BEST_EFFORT_MAY_BLOCK;
                break;
            default:
                assert false : "Invalid task type";
                return;
        }
        PostTask.postDelayedTask(traits, task, delayMs);
    }

    @Override
    public LibraryResolver getLibraryResolver() {
        return mLibraryResolver;
    }

    @Override
    public boolean isXsurfaceUsageAndCrashReportingEnabled() {
        PrivacyPreferencesManager manager = sPrivacyPreferencesManagerForTest;
        if (manager == null) {
            manager = PrivacyPreferencesManagerImpl.getInstance();
        }
        return ChromeFeatureList.isEnabled(ChromeFeatureList.XSURFACE_METRICS_REPORTING)
                && manager.isMetricsReportingEnabled();
    }

    public static Context createFeedContext(Context context) {
        return SplitCompatUtils.createContextForInflation(context, FEED_SPLIT_NAME);
    }

    @Override
    public long getReliabilityLoggingId() {
        return FeedServiceBridge.getReliabilityLoggingId();
    }

    @Override
    public String getGoogleApiKey() {
        return GoogleAPIKeys.GOOGLE_API_KEY;
    }

    @Override
    public String getChromeVersion() {
        return VersionConstants.PRODUCT_VERSION;
    }

    @Override
    public int getChromeChannel() {
        return VersionConstants.CHANNEL;
    }
}
