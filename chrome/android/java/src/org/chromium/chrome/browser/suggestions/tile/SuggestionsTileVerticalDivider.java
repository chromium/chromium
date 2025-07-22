// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions.tile;

import android.content.Context;
import android.os.Handler;
import android.util.AttributeSet;
import android.widget.RelativeLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** The View of the vertical divider between site suggestion tiles. */
@NullMarked
public class SuggestionsTileVerticalDivider extends RelativeLayout {

    /** The duration for the divider show transition, in ms. */
    private static final long SHOW_TRANSITION_MS = 400L;

    /** The duration for the divider hide transition, in ms. */
    private static final long HIDE_TRANSITION_MS = 500L;

    /** The duration for which divider is transiently visible if requested, in ms. */
    private static final long SHOW_THEN_HIDE_SUSTAIN_MS = 1200L;

    private final Handler mHandler = new Handler();

    public SuggestionsTileVerticalDivider(Context context) {
        this(context, null);
    }

    public SuggestionsTileVerticalDivider(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    private void cancelQueuedTask() {
        mHandler.removeCallbacksAndMessages(null);
        animate().cancel();
    }

    /**
     * @param state Fractional visibility state.
     * @param duration The duration of the animation in milliseconds.
     */
    private void setFractionalVisibilityAnimated(float state, long duration) {
        if (duration == 0) {
            setAlpha(state);
        } else {
            animate().setDuration(duration).alpha(state).start();
        }
    }

    /** Shows the divider, then after some delay, hides it again. */
    public void showThenHide() {
        show(true);
        mHandler.postDelayed(() -> hide(true), SHOW_THEN_HIDE_SUSTAIN_MS);
    }

    /**
     * @param isAnimated When true, animates transition; else immediately shows.
     */
    public void show(boolean isAnimated) {
        cancelQueuedTask();
        setFractionalVisibilityAnimated(1f, isAnimated ? SHOW_TRANSITION_MS : 0L);
    }

    /**
     * @param isAnimated When true, animates transition; else immediately hides.
     */
    public void hide(boolean isAnimated) {
        cancelQueuedTask();
        setFractionalVisibilityAnimated(0f, isAnimated ? HIDE_TRANSITION_MS : 0L);
    }
}
