// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.feed.ScrollListener.ScrollState;
import org.chromium.chrome.browser.xsurface.SurfaceHeaderOffsetObserver;

/**
 * Provides activity, darkmode and logging context for a single surface.
 */
public class FeedSurfaceScopeDependencyProviderImpl
        implements org.chromium.chrome.browser.xsurface.feed.FeedSurfaceScopeDependencyProvider,
                   ScrollListener {
    private static final String TAG = "Feed";
    private final Activity mActivity;
    private final Context mActivityContext;
    private final boolean mDarkMode;
    private final ObserverList<SurfaceHeaderOffsetObserver> mObserverList = new ObserverList<>();

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
}
