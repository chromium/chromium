// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import androidx.annotation.IntDef;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Helper class to check if the animation is frozen. */
@NullMarked
/* package */ class AnimationFreezeChecker {
    public static final String BACKGROUND_TAG = "BackgroundAnimation";
    public static final String FOREGROUND_RECT_TAG = "ForegroundRectAnimation";
    public static final String FOREGROUND_CORNER_TAG = "ForegroundCornerAnimation";
    public static final String FOREGROUND_FADE_TAG = "ForegroundFadeAnimation";
    public static final String FOREGROUND_EXPAND_TAG = "ForegroundExpandAnimation";

    private static final long TIMEOUT_MS = 1500L;

    // LINT.IfChange(AnimationState)
    @IntDef({
        AnimationState.STARTED,
        AnimationState.ENDED,
        AnimationState.CANCELLED,
        AnimationState.TIMED_OUT,
        AnimationState.CANCELLED_OR_ENDED_AFTER_TIMEOUT
    })
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface AnimationState {
        int STARTED = 0;
        int ENDED = 1;
        int CANCELLED = 2;
        int TIMED_OUT = 3;
        int CANCELLED_OR_ENDED_AFTER_TIMEOUT = 4;
        int ENDED_LOOPED = 5;
        int CANCELLED_LOOPED = 6;
        int NUM_ENTRIES = 7;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/tab/enums.xml:AnimationFreezeCheckerState)

    private final String mHistogramName;
    private boolean mTimedOut;
    private boolean mCancelled;
    private boolean mEnded;
    private boolean mLooped;

    AnimationFreezeChecker(String tag) {
        mHistogramName = "Tab." + tag + ".NewTabAnimationProgress";
    }

    void onAnimationStart() {
        RecordHistogram.recordEnumeratedHistogram(
                mHistogramName, AnimationState.STARTED, AnimationState.NUM_ENTRIES);
        ThreadUtils.postOnUiThreadDelayed(this::onTimeout, TIMEOUT_MS);
    }

    void onAnimationCancel() {
        onAnimationEndInternal(/* cancelled= */ true);
        mCancelled = true;
    }

    void onAnimationEnd() {
        onAnimationEndInternal(/* cancelled= */ false);
        mEnded = true;
    }

    void onTimeout() {
        if (cancelledOrEnded()) return;
        mTimedOut = true;

        RecordHistogram.recordEnumeratedHistogram(
                mHistogramName, AnimationState.TIMED_OUT, AnimationState.NUM_ENTRIES);
    }

    private void onAnimationEndInternal(boolean cancelled) {
        if (cancelledOrEnded()) {
            if (mLooped) return;

            if (mCancelled && cancelled) {
                RecordHistogram.recordEnumeratedHistogram(
                        mHistogramName,
                        AnimationState.CANCELLED_LOOPED,
                        AnimationState.NUM_ENTRIES);
                mLooped = true;
            } else if (mEnded && !cancelled) {
                RecordHistogram.recordEnumeratedHistogram(
                        mHistogramName, AnimationState.ENDED_LOOPED, AnimationState.NUM_ENTRIES);
                mLooped = true;
            }
            return;
        }

        if (mTimedOut) {
            RecordHistogram.recordEnumeratedHistogram(
                    mHistogramName,
                    AnimationState.CANCELLED_OR_ENDED_AFTER_TIMEOUT,
                    AnimationState.NUM_ENTRIES);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    mHistogramName,
                    cancelled ? AnimationState.CANCELLED : AnimationState.ENDED,
                    AnimationState.NUM_ENTRIES);
        }
    }

    private boolean cancelledOrEnded() {
        return mCancelled || mEnded;
    }
}
