// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedapplifecyclelistener;

import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.feed.library.api.internal.common.ThreadUtils;
import org.chromium.chrome.browser.feed.library.feedapplifecyclelistener.FeedLifecycleListener.LifecycleEvent;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Test class for {@link FeedAppLifecycleListener} */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedAppLifecycleListenerTest {
    @Mock
    private FeedLifecycleListener mLifecycleListener;
    @Mock
    private ThreadUtils mThreadUtils;
    private FeedAppLifecycleListener mAppLifecycleListener;

    @Before
    public void setup() {
        initMocks(this);
        mAppLifecycleListener = new FeedAppLifecycleListener(mThreadUtils);
        mAppLifecycleListener.registerObserver(mLifecycleListener);
    }

    @Test
    public void onEnterForeground() {
        mAppLifecycleListener.onEnterForeground();
        verify(mLifecycleListener).onLifecycleEvent(LifecycleEvent.ENTER_FOREGROUND);
    }

    @Test
    public void onEnterBackground() {
        mAppLifecycleListener.onEnterBackground();
        verify(mLifecycleListener).onLifecycleEvent(LifecycleEvent.ENTER_BACKGROUND);
    }

    @Test
    public void onClearAll() {
        mAppLifecycleListener.onClearAll();
        verify(mLifecycleListener).onLifecycleEvent(LifecycleEvent.CLEAR_ALL);
    }

    @Test
    public void onClearAllWithRefresh() {
        mAppLifecycleListener.onClearAllWithRefresh();
        verify(mLifecycleListener).onLifecycleEvent(LifecycleEvent.CLEAR_ALL_WITH_REFRESH);
    }

    @Test
    public void onSignedIn() {
        mAppLifecycleListener.onSignedIn();
        verify(mLifecycleListener).onLifecycleEvent(LifecycleEvent.SIGNED_IN);
    }

    @Test
    public void onSignedOut() {
        mAppLifecycleListener.onSignedOut();
        verify(mLifecycleListener).onLifecycleEvent(LifecycleEvent.SIGNED_OUT);
    }

    @Test
    public void onInitialize() {
        mAppLifecycleListener.initialize();
        verify(mLifecycleListener).onLifecycleEvent(LifecycleEvent.INITIALIZE);
    }
}
