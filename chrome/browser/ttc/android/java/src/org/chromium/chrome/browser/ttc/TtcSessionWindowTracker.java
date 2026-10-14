// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;

import java.util.HashSet;
import java.util.Set;

/**
 * Ends a profile's TTC session once no window that hosts the session UI is visible.
 *
 * <p>A session belongs to the profile, not to the window it was started from: voice focus and the
 * session UI follow the window the user is in. So one window stopping, closing or switching to
 * incognito must not end the session while another window is still visible. Microphone capture
 * can't continue in the background, so the session ends when the last visible window goes away.
 *
 * <p>There is one tracker per profile, owned by its {@link TtcKeyedService}.
 */
@NullMarked
class TtcSessionWindowTracker {
    private final Runnable mEndSession;
    private final Set<Object> mVisibleWindows = new HashSet<>();

    /**
     * @param endSession Ends the profile's session. Run when the last visible window goes away.
     */
    TtcSessionWindowTracker(Runnable endSession) {
        mEndSession = endSession;
    }

    /** Records that {@code window} is visible and hosting the session UI. Idempotent. */
    void addVisibleWindow(Object window) {
        ThreadUtils.assertOnUiThread();
        mVisibleWindows.add(window);
    }

    /**
     * Records that {@code window} no longer hosts a visible session UI, and ends the session if it
     * was the last one. No-op for a window that isn't tracked.
     */
    void removeVisibleWindow(Object window) {
        ThreadUtils.assertOnUiThread();
        if (!mVisibleWindows.remove(window)) return;
        if (mVisibleWindows.isEmpty()) mEndSession.run();
    }
}
