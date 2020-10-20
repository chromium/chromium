// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedapplifecyclelistener;

import org.chromium.chrome.browser.feed.library.api.client.lifecycle.AppLifecycleListener;
import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.common.feedobservable.FeedObservable;
import org.chromium.chrome.browser.feed.library.common.logging.Logger;
import org.chromium.chrome.browser.feed.library.feedapplifecyclelistener.FeedLifecycleListener.LifecycleEvent;

/** Default implementation of {@link AppLifecycleListener} */
public final class FeedAppLifecycleListener
        extends FeedObservable<FeedLifecycleListener> implements AppLifecycleListener {
    private static final String TAG = "FeedAppLifecycleLstnr";

    private final ThreadUtils mThreadUtils;

    public FeedAppLifecycleListener(ThreadUtils threadUtils) {
        this.mThreadUtils = threadUtils;
    }

    @Override
    public void onEnterForeground() {
        mThreadUtils.checkMainThread();
        Logger.i(TAG, "onEnterForeground called");
        dispatchLifecycleEvent(LifecycleEvent.ENTER_FOREGROUND);
    }

    @Override
    public void onEnterBackground() {
        mThreadUtils.checkMainThread();
        Logger.i(TAG, "onEnterBackground called");
        dispatchLifecycleEvent(LifecycleEvent.ENTER_BACKGROUND);
    }

    @Override
    public void onClearAll() {
        Logger.i(TAG, "onClearAll called");
        mThreadUtils.checkMainThread();
        dispatchLifecycleEvent(LifecycleEvent.CLEAR_ALL);
    }

    @Override
    public void onClearAllWithRefresh() {
        mThreadUtils.checkMainThread();
        Logger.i(TAG, "onClearAllWithRefresh called");
        dispatchLifecycleEvent(LifecycleEvent.CLEAR_ALL_WITH_REFRESH);
    }

    @Override
    public void onSignedIn() {
        mThreadUtils.checkMainThread();
        dispatchLifecycleEvent(LifecycleEvent.SIGNED_IN);
    }

    @Override
    public void onSignedOut() {
        mThreadUtils.checkMainThread();
        dispatchLifecycleEvent(LifecycleEvent.SIGNED_OUT);
    }

    @Override
    public void initialize() {
        mThreadUtils.checkMainThread();
        Logger.i(TAG, "initialize called");
        dispatchLifecycleEvent(LifecycleEvent.INITIALIZE);
    }

    private void dispatchLifecycleEvent(@LifecycleEvent String event) {
        synchronized (mObservers) {
            for (FeedLifecycleListener listener : mObservers) {
                listener.onLifecycleEvent(event);
            }
        }
    }
}
