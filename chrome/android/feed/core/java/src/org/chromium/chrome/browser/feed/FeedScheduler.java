// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import com.google.android.libraries.feed.api.host.scheduler.SchedulerApi;

/**
 * An extension of the {@link SchedulerApi} with additional methods needed for scheduling logic
 * in Chromium.
 */
public interface FeedScheduler extends SchedulerApi {
    /** Cleans up native resources, should be called when no longer needed. */
    void destroy();

    /** To be called whenever the browser is foregrounded. */
    void onForegrounded();

    /**
     * To be called when a background scheduling task wakes up the browser.
     * @param onCompletion to be run when the fixed timer logic is complete.
     */
    void onFixedTimer(Runnable onCompletion);

    /** To be called when an article is consumed, influencing future scheduling. */
    void onSuggestionConsumed();

    /**
     * To be called when articles are cleared.
     * @param suppressRefreshes whether the scheduler should temporarily avoid kicking off
     * refreshes. This is used, for example, when history data is deleted.
     * @return If a refresh should be made by the caller.
     */
    boolean onArticlesCleared(boolean suppressRefreshes);
}
