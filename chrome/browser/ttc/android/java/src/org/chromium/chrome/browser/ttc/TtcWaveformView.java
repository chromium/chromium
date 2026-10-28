// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;

/**
 * Draws the voice waveform of the large-screen TTC session UI. Mirrors the visual design of the
 * desktop voice plate's voice::WaveformView (chrome/browser/ui/views/voice/waveform_view.cc) so
 * both platforms look and move the same:
 *
 * <ul>
 *   <li>{@link Mode#IDLE}: a row of dots, used when there's no audio to show.
 *   <li>{@link Mode#ACTIVE}: vertical bars driven by the microphone level, which propagates from
 *       the center bar outwards.
 * </ul>
 *
 * <p>The view only animates while it's attached, visible, in {@link Mode#ACTIVE} and the bars are
 * moving or there's audio to show, and never when the system has animations turned off.
 */
@NullMarked
public class TtcWaveformView extends View {
    /** What the waveform shows. */
    // TODO(b/575939537): Add the desktop waveform's processing mode (a bump that travels across the
    // row of dots) once UX has decided whether, and when, the voice plate shows it.
    @IntDef({Mode.IDLE, Mode.ACTIVE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface Mode {
        int IDLE = 0;
        int ACTIVE = 1;
    }

    // Layout, in dp. Matches the desktop full-size waveform.
    @VisibleForTesting static final int BAR_COUNT = 9;
    private static final int CENTER_BAR = BAR_COUNT / 2;
    private static final float BAR_WIDTH_DP = 2f;
    // From one bar's left edge to the next one's.
    private static final float BAR_SPACING_DP = 6f;
    private static final float BAR_ROW_WIDTH_DP = (BAR_COUNT - 1) * BAR_SPACING_DP + BAR_WIDTH_DP;
    private static final float BAR_CORNER_RADIUS_DP = 0.5f;
    private static final float HORIZONTAL_PADDING_DP = 13f;
    private static final float HEIGHT_DP = 24f;
    private static final float DOT_SIZE_DP = 2f;

    // Bar heights, in dp.
    @VisibleForTesting static final float MIN_BAR_HEIGHT_DP = 4f;
    @VisibleForTesting static final float MAX_BAR_HEIGHT_DP = 20f;

    // How far a bar moves towards its target height on each frame at FRAME_RATE.
    private static final float BUBBLING_SPEED = 0.4f;
    // The desktop waveform's animation frame rate, which BUBBLING_SPEED is tuned for.
    private static final float FRAME_RATE = 60f;

    // A bar closer than this to its target height snaps to it, so that the animation can stop.
    private static final float SETTLE_DISTANCE_DP = 0.01f;

    // How often the audio level moves one bar further from the center.
    @VisibleForTesting static final long HISTORY_STEP_MS = 45;

    // Audio sensitivity and response curve.
    private static final float SILENCE_THRESHOLD = 0.005f;
    private static final float AUDIO_SENSITIVITY = 8f;
    private static final float INFLECTION_INPUT = 0.75f;
    private static final float INFLECTION_BAR_HEIGHT_DP = 0.75f * MAX_BAR_HEIGHT_DP;

    // The value of mPendingLevel when no level has been reported since the last history step.
    private static final float NO_PENDING_LEVEL = -1f;

    private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    // The bars are symmetric, so these are indexed by distance from the center bar.
    private final float[] mBarHeights = new float[CENTER_BAR + 1];
    private final float[] mTargetBarHeights = new float[CENTER_BAR + 1];
    // The audio level of each bar, most recent (the center bar's) first.
    private final float[] mAudioHistory = new float[CENTER_BAR + 1];

    private @Mode int mMode = Mode.IDLE;

    // The level reported since the last history step, if any.
    private float mPendingLevel;
    // The last level shown, which is shown for one more step if no new level arrives in time.
    private float mHeldLevel;

    private long mLastFrameTimeMs;
    private long mHistoryElapsedMs;

    public TtcWaveformView(Context context) {
        this(context, null);
    }

    public TtcWaveformView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        mPaint.setStyle(Paint.Style.FILL);
        mPaint.setColor(SemanticColorUtils.getDefaultTextColor(context));
        resetBars();
    }

    /** Sets what the waveform shows. */
    public void setMode(@Mode int mode) {
        if (mMode == mode) return;
        mMode = mode;
        if (mode != Mode.ACTIVE) resetBars();
        mLastFrameTimeMs = now();
        invalidate();
    }

    /** Reports the microphone level, in [0, 1]. Ignored unless in {@link Mode#ACTIVE}. */
    public void setAudioLevel(float level) {
        // Without animations the bars don't move, so there's nothing to redraw.
        if (mMode != Mode.ACTIVE || !ValueAnimator.areAnimatorsEnabled()) return;
        // Also turns NaN into silence.
        float clampedLevel = level > 0f ? Math.min(1f, level) : 0f;
        boolean wasAtRest = isAtRest();
        // A level too quiet to raise a bar doesn't need to wake up the animation.
        if (wasAtRest && !raisesBar(clampedLevel)) return;
        mPendingLevel = clampedLevel;
        // The time spent at rest isn't one long frame.
        if (wasAtRest) mLastFrameTimeMs = now();
        invalidate();
    }

    @Mode
    int getModeForTesting() {
        return mMode;
    }

    /** Returns the current height of each bar, in dp. */
    float[] getBarHeightsForTesting() {
        float[] heights = new float[BAR_COUNT];
        for (int i = 0; i < BAR_COUNT; i++) heights[i] = getBarHeight(i);
        return heights;
    }

    boolean isAtRestForTesting() {
        return isAtRest();
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        float density = getResources().getDisplayMetrics().density;
        float widthDp = BAR_ROW_WIDTH_DP + HORIZONTAL_PADDING_DP;
        int width = Math.round(widthDp * density);
        int height = Math.round(HEIGHT_DP * density);
        setMeasuredDimension(
                resolveSize(width, widthMeasureSpec), resolveSize(height, heightMeasureSpec));
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mLastFrameTimeMs = now();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        long now = now();
        long deltaMs = Math.max(0, now - mLastFrameTimeMs);
        mLastFrameTimeMs = now;
        boolean animate = mMode == Mode.ACTIVE && ValueAnimator.areAnimatorsEnabled();
        if (animate) updateBars(deltaMs);

        // Read on every draw, since the window can move to a display with a different density.
        float density = getResources().getDisplayMetrics().density;
        float barSpacing = BAR_SPACING_DP * density;
        float barWidth = BAR_WIDTH_DP * density;
        float firstBarLeft = (getWidth() - BAR_ROW_WIDTH_DP * density) / 2f;
        float centerY = getHeight() / 2f;
        if (mMode == Mode.ACTIVE) {
            float cornerRadius = BAR_CORNER_RADIUS_DP * density;
            for (int i = 0; i < BAR_COUNT; i++) {
                float left = firstBarLeft + i * barSpacing;
                float halfHeight = getBarHeight(i) * density / 2f;
                canvas.drawRoundRect(
                        left,
                        centerY - halfHeight,
                        left + barWidth,
                        centerY + halfHeight,
                        cornerRadius,
                        cornerRadius,
                        mPaint);
            }
        } else {
            float firstDotCenterX = firstBarLeft + barWidth / 2f;
            float dotRadius = DOT_SIZE_DP * density / 2f;
            for (int i = 0; i < BAR_COUNT; i++) {
                canvas.drawCircle(firstDotCenterX + i * barSpacing, centerY, dotRadius, mPaint);
            }
        }

        if (animate && isShown() && !isAtRest()) postInvalidateOnAnimation();
    }

    /** Advances the bars by {@code deltaMs}: steps the audio history and eases the heights. */
    @VisibleForTesting
    void updateBars(long deltaMs) {
        mHistoryElapsedMs += deltaMs;
        if (mHistoryElapsedMs >= HISTORY_STEP_MS) {
            // Keep the remainder so that steps average HISTORY_STEP_MS whatever the frame rate.
            mHistoryElapsedMs %= HISTORY_STEP_MS;
            stepAudioHistory();
        }
        // Scaled so that the bars move at the same speed whatever the display's refresh rate.
        float frames = deltaMs * FRAME_RATE / 1000f;
        float easing = 1f - (float) Math.pow(1f - BUBBLING_SPEED, frames);
        for (int distance = 0; distance <= CENTER_BAR; distance++) {
            float remaining = mTargetBarHeights[distance] - mBarHeights[distance];
            if (Math.abs(remaining) < SETTLE_DISTANCE_DP) {
                mBarHeights[distance] = mTargetBarHeights[distance];
            } else {
                mBarHeights[distance] += remaining * easing;
            }
        }
    }

    /**
     * Moves each audio level one bar further from the center, puts the latest level on the center
     * bar, and updates the target heights to match.
     */
    private void stepAudioHistory() {
        System.arraycopy(mAudioHistory, 0, mAudioHistory, 1, mAudioHistory.length - 1);
        if (mPendingLevel != NO_PENDING_LEVEL) {
            mAudioHistory[0] = mPendingLevel;
            mHeldLevel = mPendingLevel;
            mPendingLevel = NO_PENDING_LEVEL;
        } else {
            // Hold the last level for one step so a briefly late sample doesn't flicker.
            mAudioHistory[0] = mHeldLevel;
            mHeldLevel = 0f;
        }
        for (int distance = 0; distance <= CENTER_BAR; distance++) {
            float taper = 1f - distance / (CENTER_BAR + 1f);
            float height = calculateBarHeight(mAudioHistory[distance]) * taper;
            mTargetBarHeights[distance] = Math.max(MIN_BAR_HEIGHT_DP, height);
        }
    }

    /**
     * Whether nothing would move: the bars are at the minimum height, and neither the levels on
     * their way out from the center nor the next ones would raise them.
     */
    private boolean isAtRest() {
        if (raisesBar(mPendingLevel) || raisesBar(mHeldLevel)) return false;
        // The levels only get further from the center, where they're tapered more, so a level
        // that doesn't raise its bar now never will.
        for (int distance = 0; distance <= CENTER_BAR; distance++) {
            if (mBarHeights[distance] > MIN_BAR_HEIGHT_DP) return false;
            if (mTargetBarHeights[distance] > MIN_BAR_HEIGHT_DP) return false;
        }
        return true;
    }

    private float getBarHeight(int index) {
        return mBarHeights[Math.abs(index - CENTER_BAR)];
    }

    private void resetBars() {
        Arrays.fill(mBarHeights, MIN_BAR_HEIGHT_DP);
        Arrays.fill(mTargetBarHeights, MIN_BAR_HEIGHT_DP);
        Arrays.fill(mAudioHistory, 0f);
        mPendingLevel = NO_PENDING_LEVEL;
        mHeldLevel = 0f;
        mHistoryElapsedMs = 0;
    }

    private static boolean raisesBar(float level) {
        return calculateBarHeight(level) > MIN_BAR_HEIGHT_DP;
    }

    /** Maps a microphone level in [0, 1] to a bar height in dp. */
    @VisibleForTesting
    static float calculateBarHeight(float level) {
        float loudness = (float) Math.sqrt(Math.max(0f, Math.min(1f, level * AUDIO_SENSITIVITY)));
        if (loudness <= SILENCE_THRESHOLD) return MIN_BAR_HEIGHT_DP;
        if (loudness < INFLECTION_INPUT) {
            return mapRange(
                    loudness,
                    SILENCE_THRESHOLD,
                    INFLECTION_INPUT,
                    MIN_BAR_HEIGHT_DP,
                    INFLECTION_BAR_HEIGHT_DP);
        }
        return mapRange(
                loudness, INFLECTION_INPUT, 1f, INFLECTION_BAR_HEIGHT_DP, MAX_BAR_HEIGHT_DP);
    }

    private static float mapRange(
            float value, float fromMin, float fromMax, float toMin, float toMax) {
        return toMin + (value - fromMin) / (fromMax - fromMin) * (toMax - toMin);
    }

    private static long now() {
        return SystemClock.uptimeMillis();
    }
}
