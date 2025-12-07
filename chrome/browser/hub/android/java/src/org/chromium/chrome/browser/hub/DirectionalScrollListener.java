// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.os.Handler;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;

/**
 * A {@link RecyclerView.OnScrollListener} that detects scroll direction and invokes a callback with
 * throttling.
 */
@NullMarked
public class DirectionalScrollListener extends RecyclerView.OnScrollListener {
    private final Runnable mOnScrollUp;
    private final Runnable mOnScrollDown;
    private final long mThrottleMs;
    private final int mScrollUpThreshold;
    private final int mScrollDownThreshold;
    private final Handler mHandler;
    private boolean mIsThrottled;

    private static final int DEFAULT_THROTTLE_MS = 50;
    private static final int DEFAULT_SCROLL_UP_THRESHOLD = 5;
    private static final int DEFAULT_SCROLL_DOWN_THRESHOLD = 2;

    /**
     * Creates a new instance of {@link DirectionalScrollListener}.
     *
     * @param onScrollUp The callback to invoke when scrolling up.
     * @param onScrollDown The callback to invoke when scrolling down.
     */
    public DirectionalScrollListener(Runnable onScrollUp, Runnable onScrollDown) {
        this(
                onScrollUp,
                onScrollDown,
                DEFAULT_THROTTLE_MS,
                DEFAULT_SCROLL_UP_THRESHOLD,
                DEFAULT_SCROLL_DOWN_THRESHOLD);
    }

    /**
     * Creates a new instance of {@link DirectionalScrollListener}.
     *
     * @param onScrollUp The callback to invoke when scrolling up.
     * @param onScrollDown The callback to invoke when scrolling down.
     * @param throttleMs The minimum time in milliseconds between invocations of the callbacks.
     */
    public DirectionalScrollListener(Runnable onScrollUp, Runnable onScrollDown, long throttleMs) {
        this(
                onScrollUp,
                onScrollDown,
                throttleMs,
                DEFAULT_SCROLL_UP_THRESHOLD,
                DEFAULT_SCROLL_DOWN_THRESHOLD);
    }

    /**
     * Creates a new instance of {@link DirectionalScrollListener}.
     *
     * @param onScrollUp The callback to invoke when scrolling up.
     * @param onScrollDown The callback to invoke when scrolling down.
     * @param throttleMs The minimum time in milliseconds between invocations of the callbacks.
     * @param scrollThreshold The minimum scroll distance in pixels to trigger the callbacks.
     */
    public DirectionalScrollListener(
            Runnable onScrollUp, Runnable onScrollDown, long throttleMs, int scrollThreshold) {
        this(onScrollUp, onScrollDown, throttleMs, scrollThreshold, scrollThreshold);
    }

    /**
     * Creates a new instance of {@link DirectionalScrollListener}.
     *
     * @param onScrollUp The callback to invoke when scrolling up.
     * @param onScrollDown The callback to invoke when scrolling down.
     * @param throttleMs The minimum time in milliseconds between invocations of the callbacks.
     * @param scrollUpThreshold The minimum scroll up distance in pixels to trigger the callbacks.
     * @param scrollDownThreshold The minimum scroll down distance in pixels to trigger the
     *     callbacks.
     */
    public DirectionalScrollListener(
            Runnable onScrollUp,
            Runnable onScrollDown,
            long throttleMs,
            int scrollUpThreshold,
            int scrollDownThreshold) {
        mOnScrollUp = onScrollUp;
        mOnScrollDown = onScrollDown;
        mThrottleMs = throttleMs;
        mScrollUpThreshold = scrollUpThreshold;
        mScrollDownThreshold = scrollDownThreshold;
        mHandler = new Handler();
    }

    @Override
    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
        if (mIsThrottled) {
            return;
        }

        if (dy > 0) {
            if (dy < mScrollDownThreshold) {
                return;
            }
            mOnScrollDown.run();
        } else if (dy < 0) {
            if (Math.abs(dy) < mScrollUpThreshold) {
                return;
            }
            mOnScrollUp.run();
        } else {
            return;
        }

        mIsThrottled = true;
        mHandler.postDelayed(() -> mIsThrottled = false, mThrottleMs);
    }
}
