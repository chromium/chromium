// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.library.feedapplifecyclelistener;

import androidx.annotation.StringDef;

/**
 * Internal interface used to register Feed components with the {@link FeedAppLifecycleListener},
 * which is used by consuming hosts to communicate app lifecycle events to the Feed Library.
 */
public interface FeedLifecycleListener {
    /** The types of lifecycle events. */
    @StringDef({LifecycleEvent.ENTER_FOREGROUND, LifecycleEvent.ENTER_BACKGROUND,
            LifecycleEvent.CLEAR_ALL, LifecycleEvent.CLEAR_ALL_WITH_REFRESH,
            LifecycleEvent.INITIALIZE, LifecycleEvent.SIGNED_IN, LifecycleEvent.SIGNED_OUT})
    @interface LifecycleEvent {
        String ENTER_FOREGROUND = "foreground";
        String ENTER_BACKGROUND = "background";
        String CLEAR_ALL = "clearAll";
        String CLEAR_ALL_WITH_REFRESH = "clearAllWithRefresh";
        String INITIALIZE = "initialize";
        String SIGNED_IN = "signedIn";
        String SIGNED_OUT = "signedOut";
    }

    void onLifecycleEvent(@LifecycleEvent String event);
}
