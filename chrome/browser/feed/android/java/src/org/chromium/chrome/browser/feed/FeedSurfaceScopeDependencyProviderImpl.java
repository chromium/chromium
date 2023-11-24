// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.xsurface.LoggingParameters;
import org.chromium.chrome.browser.xsurface.PersistentKeyValueCache;
import org.chromium.chrome.browser.xsurface.SurfaceHeaderOffsetObserver;
import org.chromium.chrome.browser.xsurface.feed.ResourceFetcher;
import org.chromium.url.GURL;

/** Provides activity, darkmode and logging context for a single surface. */
@JNINamespace("feed::android")
public class FeedSurfaceScopeDependencyProviderImpl
        implements org.chromium.chrome.browser.xsurface.feed.FeedSurfaceScopeDependencyProvider,
                ScrollListener {
    public static class NetworkResponse {
        public boolean success;
        public int statusCode;
        public String[] headerNameAndValues;
        public @Nullable byte[] rawData;

        @CalledByNative("NetworkResponse")
        public NetworkResponse(
                boolean success,
                int statusCode,
                String[] headerNameAndValues,
                @Nullable byte[] rawData) {
            this.success = success;
            this.statusCode = statusCode;
            this.headerNameAndValues = headerNameAndValues;
            this.rawData = rawData;
        }
    }

    private final Activity mActivity;
    private final Context mActivityContext;
    private final boolean mDarkMode;
    private final ObserverList<SurfaceHeaderOffsetObserver> mObserverList = new ObserverList<>();
    private final FeedPersistentKeyValueCache mPersistentKeyValueCache =
            new FeedPersistentKeyValueCache();

    public FeedSurfaceScopeDependencyProviderImpl(
            Activity activity, Context activityContext, boolean darkMode) {
        mActivityContext = FeedProcessScopeDependencyProvider.createFeedContext(activityContext);
        mDarkMode = darkMode;
        mActivity = activity;
    }

    @Override
    public Activity getActivity() {
        return mActivity;
    }

    @Override
    public Context getActivityContext() {
        return mActivityContext;
    }

    @Override
    public boolean isDarkModeEnabled() {
        return mDarkMode;
    }

    @Override
    public Rect getToolbarGlobalVisibleRect() {
        Rect bounds = new Rect();
        View toolbarView = mActivity.findViewById(R.id.toolbar);
        if (toolbarView == null) {
            return bounds;
        }
        toolbarView.getGlobalVisibleRect(bounds);
        return bounds;
    }

    @Override
    public void onScrollStateChanged(@ScrollState int state) {}

    @Override
    public void onScrolled(int dx, int dy) {}

    @Override
    public void onHeaderOffsetChanged(int verticalOffset) {
        for (SurfaceHeaderOffsetObserver observer : mObserverList) {
            observer.onHeaderOffsetChanged(verticalOffset);
        }
    }

    @Override
    public void addHeaderOffsetObserver(SurfaceHeaderOffsetObserver observer) {
        mObserverList.addObserver(observer);
    }

    @Override
    public void removeHeaderOffsetObserver(SurfaceHeaderOffsetObserver observer) {
        mObserverList.removeObserver(observer);
    }

    @Override
    public void processViewAction(byte[] data, LoggingParameters loggingParameters) {
        FeedProcessScopeDependencyProviderJni.get()
                .processViewAction(
                        data,
                        FeedLoggingParameters.convertToProto(loggingParameters).toByteArray());
    }

    @Override
    public PersistentKeyValueCache getPersistentKeyValueCache() {
        return mPersistentKeyValueCache;
    }

    @Override
    public ResourceFetcher getAsyncDataFetcher() {
        return new FeedResourceFetcher();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        void fetchResource(
                GURL url,
                String method,
                String[] headerNameAndValues,
                byte[] postData,
                Callback<NetworkResponse> callback);
    }
}
