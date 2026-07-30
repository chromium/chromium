// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import org.junit.Assert;

import org.chromium.android_webview.AwNavigation;
import org.chromium.android_webview.AwNavigationListener;
import org.chromium.android_webview.AwPage;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.build.annotations.Nullable;

import java.lang.reflect.InvocationHandler;
import java.util.ArrayList;
import java.util.List;

/** AwNavigationListener subclass used for testing. */
public class TestAwNavigationListener implements AwNavigationListener {
    private final List<AwNavigation> mStartedNavigations = new ArrayList<AwNavigation>();
    private final List<AwNavigation> mRedirectedNavigations = new ArrayList<AwNavigation>();
    private final List<AwNavigation> mCompletedNavigations = new ArrayList<AwNavigation>();
    private final List<AwNavigation> mVisibleNavigations = new ArrayList<AwNavigation>();
    private final List<AwPage> mDeletedPages = new ArrayList<AwPage>();
    private final List<AwPage> mPagesWithLoadEventFired = new ArrayList<AwPage>();
    private final List<AwPage> mPagesWithDOMContentLoadEventFired = new ArrayList<AwPage>();
    private final List<Long> mFirstContentfulPaintLoadTimes = new ArrayList<Long>();
    private final List<Long> mLargestContentfulPaintLoadTimes = new ArrayList<Long>();
    private final List<PerformanceMark> mPerformanceMarks = new ArrayList<PerformanceMark>();

    private final CallbackHelper mCallbackHelper;

    public TestAwNavigationListener(CallbackHelper callbackHelper) {
        mCallbackHelper = callbackHelper;
    }

    @Nullable AwNavigation getLastStartedNavigation() {
        if (mStartedNavigations.isEmpty()) {
            return null;
        }
        return mStartedNavigations.get(mStartedNavigations.size() - 1);
    }

    @Nullable AwNavigation getLastRedirectedNavigation() {
        if (mRedirectedNavigations.isEmpty()) {
            return null;
        }
        return mRedirectedNavigations.get(mRedirectedNavigations.size() - 1);
    }

    @Nullable AwNavigation getLastCompletedNavigation() {
        if (mCompletedNavigations.isEmpty()) {
            return null;
        }
        return mCompletedNavigations.get(mCompletedNavigations.size() - 1);
    }

    @Nullable AwNavigation getLastVisibleNavigation() {
        if (mVisibleNavigations.isEmpty()) {
            return null;
        }
        return mVisibleNavigations.get(mVisibleNavigations.size() - 1);
    }

    @Nullable AwPage getLastDeletedPage() {
        if (mDeletedPages.isEmpty()) {
            return null;
        }
        return mDeletedPages.get(mDeletedPages.size() - 1);
    }

    @Nullable AwPage getLastPageWithLoadEventFired() {
        if (mPagesWithLoadEventFired.isEmpty()) {
            return null;
        }
        return mPagesWithLoadEventFired.get(mPagesWithLoadEventFired.size() - 1);
    }

    @Nullable AwPage getLastPageWithDOMContentLoadedEventFired() {
        if (mPagesWithDOMContentLoadEventFired.isEmpty()) {
            return null;
        }
        return mPagesWithDOMContentLoadEventFired.get(
                mPagesWithDOMContentLoadEventFired.size() - 1);
    }

    @Nullable Long getLastFirstContentfulPaintLoadTime() {
        if (mFirstContentfulPaintLoadTimes.isEmpty()) {
            return null;
        }
        return mFirstContentfulPaintLoadTimes.get(mFirstContentfulPaintLoadTimes.size() - 1);
    }

    @Nullable List<Long> getLastLargestContentfulPaintLoadTimes() {
        return mLargestContentfulPaintLoadTimes;
    }

    @Nullable List<PerformanceMark> getPerformanceMarks() {
        return mPerformanceMarks;
    }

    @Override
    public /* WebViewNavigationClient */ InvocationHandler getSupportLibInvocationHandler() {
        return null;
    }

    @Override
    public void onNavigationStarted(AwNavigation navigation) {
        Assert.assertFalse(
                "onNavigationStarted should not be called twice for the same navigation",
                mStartedNavigations.contains(navigation));
        mStartedNavigations.add(navigation);
    }

    @Override
    public void onNavigationRedirected(AwNavigation navigation) {
        Assert.assertTrue(
                "onNavigationRedirected should only be called for a started navigation",
                mStartedNavigations.contains(navigation));
        mRedirectedNavigations.add(navigation);
    }

    @Override
    public void onNavigationCompleted(AwNavigation navigation) {
        Assert.assertTrue(
                "onNavigationCompleted should only be called for a started navigation",
                mStartedNavigations.contains(navigation));
        Assert.assertFalse(
                "onNavigationCompleted should not be called twice for the same navigation",
                mCompletedNavigations.contains(navigation));
        mCompletedNavigations.add(navigation);
    }

    @Override
    public void onNavigationVisible(AwNavigation navigation) {
        Assert.assertTrue(
                "onNavigationVisible should only be called for a completed navigation",
                mCompletedNavigations.contains(navigation));
        Assert.assertFalse(
                "onNavigationVisible should not be called twice for the same navigation",
                mVisibleNavigations.contains(navigation));
        mVisibleNavigations.add(navigation);
    }

    @Override
    public void onPageDeleted(AwPage page) {
        mDeletedPages.add(page);
    }

    @Override
    public void onPageLoadEventFired(AwPage page) {
        mPagesWithLoadEventFired.add(page);
    }

    @Override
    public void onPageDOMContentLoadedEventFired(AwPage page) {
        mPagesWithDOMContentLoadEventFired.add(page);
    }

    @Override
    public void onFirstContentfulPaint(AwPage page, long durationMs) {
        mFirstContentfulPaintLoadTimes.add(durationMs);
        mCallbackHelper.notifyCalled();
    }

    @Override
    public void onLargestContentfulPaint(AwPage page, long durationMs) {
        mLargestContentfulPaintLoadTimes.add(durationMs);
    }

    @Override
    public void onPerformanceMark(AwPage page, String markName, long markTimeMs) {
        mPerformanceMarks.add(new PerformanceMark(markName, markTimeMs));
        mCallbackHelper.notifyCalled();
    }

    public static class PerformanceMark {
        public String markName;
        public long markTimeMs;

        public PerformanceMark(String name, long timeMs) {
            markName = name;
            markTimeMs = timeMs;
        }
    }
}
