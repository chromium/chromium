// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import org.junit.Assert;

import org.chromium.android_webview.AwNavigation;
import org.chromium.android_webview.AwNavigationClient;
import org.chromium.android_webview.AwPage;
import org.chromium.build.annotations.Nullable;

import java.lang.reflect.InvocationHandler;
import java.util.ArrayList;
import java.util.List;

/** AwNavigationClient subclass used for testing. */
public class TestAwNavigationClient implements AwNavigationClient {
    private final List<AwNavigation> mStartedNavigations = new ArrayList<AwNavigation>();
    private final List<AwNavigation> mRedirectedNavigations = new ArrayList<AwNavigation>();
    private final List<AwNavigation> mCompletedNavigations = new ArrayList<AwNavigation>();
    private final List<AwPage> mDeletedPages = new ArrayList<AwPage>();
    private final List<AwPage> mPagesWithLoadEventFired = new ArrayList<AwPage>();
    private final List<AwPage> mPagesWithDOMContentLoadEventFired = new ArrayList<AwPage>();
    private final List<AwPage> mPagesWithFirstContentfulPaint = new ArrayList<AwPage>();

    public TestAwNavigationClient() {}

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

    @Nullable AwPage getLastPageWithFirstContentfulPaint() {
        if (mPagesWithFirstContentfulPaint.isEmpty()) {
            return null;
        }
        return mPagesWithFirstContentfulPaint.get(mPagesWithFirstContentfulPaint.size() - 1);
    }

    @Override
    public /* WebViewNavigationClient */ InvocationHandler getSupportLibInvocationHandler() {
        return null;
    }

    @Override
    public void onNavigationStarted(AwNavigation navigation) {
        for (AwNavigation startedNav : mStartedNavigations) {
            Assert.assertNotEquals(
                    "onNavigationStarted should not be called twice for the same navigation",
                    startedNav,
                    navigation);
        }
        mStartedNavigations.add(navigation);
    }

    @Override
    public void onNavigationRedirected(AwNavigation navigation) {
        boolean foundMatchingStartedNav = false;
        for (AwNavigation startedNav : mStartedNavigations) {
            if (startedNav == navigation) {
                foundMatchingStartedNav = true;
            }
        }
        Assert.assertTrue(
                "onNavigationRedirected should only be called for a started navigation",
                foundMatchingStartedNav);
        mRedirectedNavigations.add(navigation);
    }

    @Override
    public void onNavigationCompleted(AwNavigation navigation) {
        boolean foundMatchingStartedNav = false;
        for (AwNavigation startedNav : mStartedNavigations) {
            if (startedNav == navigation) {
                foundMatchingStartedNav = true;
            }
        }
        Assert.assertTrue(
                "onNavigationCompleted should only be called for a started navigation",
                foundMatchingStartedNav);
        for (AwNavigation completedNav : mCompletedNavigations) {
            Assert.assertNotEquals(
                    "onNavigationCompleted should not be called twice for the same navigation",
                    completedNav,
                    navigation);
        }
        mCompletedNavigations.add(navigation);
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
    public void onFirstContentfulPaint(AwPage page) {
        mPagesWithFirstContentfulPaint.add(page);
    }
}
