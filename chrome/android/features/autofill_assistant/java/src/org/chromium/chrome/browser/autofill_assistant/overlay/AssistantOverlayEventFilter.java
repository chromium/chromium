// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.overlay;

import android.content.Context;
import android.graphics.RectF;
import android.view.GestureDetector;
import android.view.GestureDetector.SimpleOnGestureListener;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilter;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;

/**
 * Filters gestures that happen on the overlay.
 */
class AssistantOverlayEventFilter extends EventFilter {
    /** A mode that describes what's happening to the current gesture. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({GestureMode.NONE, GestureMode.TRACKING, GestureMode.FORWARDING})
    private @interface GestureMode {
        /** There's no current gesture. */
        int NONE = 0;
        /**
         * The current gesture is being tracked and buffered. The gesture might later on transition
         * to forwarding mode or it might be abandoned.
         */
        int TRACKING = 1;
        /** The current gesture is being forwarded to the content view. */
        int FORWARDING = 2;
    }

    private AssistantOverlayDelegate mDelegate;
    private ChromeFullscreenManager mFullscreenManager;
    private View mCompositorView;

    /**
     * Complain after there's been {@link #mTapTrackingCount} taps within
     * {@link #mTapTrackingDurationMs} in the unallowed area.
     *
     * <p>The feature is disabled unless both are positive.
     */
    private int mTapTrackingCount;

    /**
     * How long to wait before resetting the tracking duration.
     */
    private long mTapTrackingDurationMs;

    /**
     * When in partial mode, let through scroll and pinch/zoom.
     *
     * Let through all events in {@link mTouchableArea}.
     */
    private boolean mPartial;

    /**
     * Coordinates of the visual viewport within the page, if known, in CSS pixels relative to the
     * origin of the page. This is used to convert pixel coordinates to CSS coordinates.
     *
     * The visual viewport includes the portion of the page that is really visible, excluding any
     * area not fully visible because of the current zoom value.
     */
    private final RectF mVisualViewport = new RectF();

    /** Touchable area, expressed in CSS pixels relative to the layout viewport. */
    private List<RectF> mTouchableArea = Collections.emptyList();

    /** Restricted area, expressed in CSS pixels relative to the layout viewport. */
    private List<RectF> mRestrictedArea = Collections.emptyList();

    /**
     * Detects taps: {@link GestureDetector#onTouchEvent} returns {@code true} after a tap event.
     */
    private final GestureDetector mTapDetector;

    /**
     * Detects scrolls and flings: {@link GestureDetector#onTouchEvent} returns {@code true} a
     * scroll or fling event.
     */
    private final GestureDetector mScrollDetector;

    /** The current state of the gesture filter. */
    @GestureMode
    private int mCurrentGestureMode;

    /**
     * A capture of the motion event that are part of the current gesture, kept around in case they
     * need to be forwarded while {@code mCurrentGestureMode == TRACKING_GESTURE_MODE}.
     *
     * <p>Elements of this list must be recycled. Call {@link #cleanupCurrentGestureBuffer}.
     */
    private List<MotionEvent> mCurrentGestureBuffer = new ArrayList<>();

    /** Times, in millisecond, of unexpected taps detected outside of the allowed area. */
    private final List<Long> mUnexpectedTapTimes = new ArrayList<>();

    AssistantOverlayEventFilter(
            Context context, ChromeFullscreenManager fullscreenManager, View compositorView) {
        super(context);

        mFullscreenManager = fullscreenManager;
        mCompositorView = compositorView;

        mTapDetector = new GestureDetector(context, new SimpleOnGestureListener() {
            @Override
            public boolean onSingleTapUp(MotionEvent e) {
                return true;
            }
        });
        mScrollDetector = new GestureDetector(context, new SimpleOnGestureListener() {
            @Override
            public boolean onScroll(
                    MotionEvent e1, MotionEvent e2, float distanceX, float distanceY) {
                return true;
            }

            @Override
            public boolean onFling(
                    MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
                return true;
            }
        });
    }

    void destroy() {
        cleanupCurrentGestureBuffer();
    }

    /** Resets tap times and current gestures. */
    void reset() {
        resetCurrentGesture();
        mUnexpectedTapTimes.clear();
    }

    /** Enables or disables partial mode. */
    void setPartial(boolean partial) {
        mPartial = partial;
    }

    void setDelegate(AssistantOverlayDelegate delegate) {
        mDelegate = delegate;
    }

    void setTapTrackingCount(int count) {
        mTapTrackingCount = count;
    }

    void setTapTrackingDurationMs(long durationMs) {
        mTapTrackingDurationMs = durationMs;
    }

    /**
     * Set the touchable area. This only applies if current state is AssistantOverlayState.PARTIAL.
     */
    void setTouchableArea(List<RectF> touchableArea) {
        mTouchableArea = touchableArea;
    }

    /**
     * Set the restricted area. This only applies if current state is AssistantOverlayState.PARTIAL.
     */
    void setRestrictedArea(List<RectF> restrictedArea) {
        mRestrictedArea = restrictedArea;
    }

    /** Sets the visual viewport. */
    void setVisualViewport(RectF visualViewport) {
        mVisualViewport.set(visualViewport);
    }

    @Override
    protected boolean onInterceptTouchEventInternal(MotionEvent event, boolean isKeyboardShowing) {
        // All events should be sent to onTouchEvent().
        return true;
    }

    @Override
    protected boolean onTouchEventInternal(MotionEvent event) {
        if (mPartial) {
            return onTouchEventWithPartialOverlay(event);
        } else {
            return onTouchEventWithFullOverlay(event);
        }
    }

    private boolean onTouchEventWithFullOverlay(MotionEvent event) {
        if (mTapDetector.onTouchEvent(event)) {
            onUnexpectedTap(event);
        }
        return true;
    }

    /**
     * Let through or intercept gestures.
     *
     * <p>If the event starts a gesture, with ACTION_DOWN, within a touchable area, let the event
     * through.
     *
     * <p>If the event starts a gesture outside a touchable area, forward it to the compositor once
     * it's clear that it's a scroll, fling or multi-touch event - and not a tap event.
     *
     * @return true if the event was handled by this view, as defined for {@link
     *         View#onTouchEvent}
     */
    private boolean onTouchEventWithPartialOverlay(MotionEvent event) {
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN: // Starts a new gesture.

                // Reset is needed, as ACTION_DOWN can interrupt a running gesture
                resetCurrentGesture();

                if (shouldLetEventThrough(event)) {
                    if (mDelegate != null) {
                        mDelegate.onUserInteractionInsideTouchableArea();
                    }
                    // This is the last we'll hear of this gesture unless it turns multi-touch. No
                    // need to track or forward it.
                    return false;
                }
                if (event.getPointerCount() > 0 && event.getPointerId(0) != 0) {
                    // We're being offered a previously let-through gesture, which turned
                    // multi-touch. This isn't a real gesture start.
                    return false;
                }

                // Track the gesture in case this is a tap, which we should handle, or a
                // scroll/fling/pinch, which we should forward.
                mCurrentGestureMode = GestureMode.TRACKING;
                mCurrentGestureBuffer.add(MotionEvent.obtain(event));
                mScrollDetector.onTouchEvent(event);
                mTapDetector.onTouchEvent(event);
                return true;

            case MotionEvent.ACTION_MOVE: // Continues a gesture.
                switch (mCurrentGestureMode) {
                    case GestureMode.TRACKING:
                        if (mScrollDetector.onTouchEvent(event)) {
                            // The current gesture is a scroll or a fling. Forward it.
                            startForwardingGesture(event);
                            return true;
                        }

                        // Continue accumulating events.
                        mTapDetector.onTouchEvent(event);
                        mCurrentGestureBuffer.add(MotionEvent.obtain(event));
                        return true;

                    case GestureMode.FORWARDING:
                        mCompositorView.dispatchTouchEvent(event);
                        return true;

                    default:
                        return true;
                }

            case MotionEvent.ACTION_POINTER_DOWN: // Continues a multi-touch gesture
            case MotionEvent.ACTION_POINTER_UP:
                switch (mCurrentGestureMode) {
                    case GestureMode.TRACKING:
                        // The current gesture has just become a multi-touch gesture. Forward it.
                        startForwardingGesture(event);
                        return true;

                    case GestureMode.FORWARDING:
                        mCompositorView.dispatchTouchEvent(event);
                        return true;

                    default:
                        return true;
                }

            case MotionEvent.ACTION_UP: // Ends a gesture
            case MotionEvent.ACTION_CANCEL:
                switch (mCurrentGestureMode) {
                    case GestureMode.TRACKING:
                        if (mTapDetector.onTouchEvent(event)) {
                            onUnexpectedTap(event);
                        }
                        resetCurrentGesture();
                        return true;

                    case GestureMode.FORWARDING:
                        mCompositorView.dispatchTouchEvent(event);
                        resetCurrentGesture();
                        return true;

                    default:
                        return true;
                }

            default:
                return true;
        }
    }

    /** Clears all information about the current gesture. */
    private void resetCurrentGesture() {
        mCurrentGestureMode = GestureMode.NONE;
        cleanupCurrentGestureBuffer();
    }

    /** Clears {@link #mCurrentGestureStart}, recycling it if necessary. */
    private void cleanupCurrentGestureBuffer() {
        for (MotionEvent event : mCurrentGestureBuffer) {
            event.recycle();
        }
        mCurrentGestureBuffer.clear();
    }

    /** Enables forwarding of the current gesture, starting with {@link currentEvent}. */
    private void startForwardingGesture(MotionEvent currentEvent) {
        mCurrentGestureMode = GestureMode.FORWARDING;
        for (MotionEvent event : mCurrentGestureBuffer) {
            mCompositorView.dispatchTouchEvent(event);
        }
        cleanupCurrentGestureBuffer();
        mCompositorView.dispatchTouchEvent(currentEvent);
    }

    /**
     * Returns {@code true} if {@code event} is for a position in the touchable area
     * or the top/bottom bar.
     */
    private boolean shouldLetEventThrough(MotionEvent event) {
        int yTop = (int) mFullscreenManager.getContentOffset();
        int height = mCompositorView.getHeight() - getBottomBarHeight() - yTop;
        return isInTouchableArea(event.getX(), event.getY() - yTop);
    }

    /** Considers whether to let the client know about unexpected taps. */
    private void onUnexpectedTap(MotionEvent e) {
        if (mTapTrackingCount <= 0 || mTapTrackingDurationMs <= 0) return;

        long eventTimeMs = e.getEventTime();
        for (Iterator<Long> iter = mUnexpectedTapTimes.iterator(); iter.hasNext();) {
            Long timeMs = iter.next();
            if ((eventTimeMs - timeMs) >= mTapTrackingDurationMs) {
                iter.remove();
            }
        }
        mUnexpectedTapTimes.add(eventTimeMs);
        if (mUnexpectedTapTimes.size() == mTapTrackingCount && mDelegate != null) {
            mDelegate.onUnexpectedTaps();
            mUnexpectedTapTimes.clear();
        }
    }

    private boolean isInTouchableArea(float x, float y) {
        if (mVisualViewport.isEmpty() || mTouchableArea.isEmpty()) return false;

        // Ratio of to use to convert physical pixels to zoomed CSS pixels. Aspect ratio is
        // conserved, so width and height are always converted with the same value. Using width
        // here, since viewport width always corresponds to the overlay width.
        float physicalPixelsToCss =
                ((float) mVisualViewport.width()) / ((float) mCompositorView.getWidth());
        float absoluteXCss = (x * physicalPixelsToCss) + mVisualViewport.left;
        float absoluteYCss = (y * physicalPixelsToCss) + mVisualViewport.top;

        for (RectF rect : mRestrictedArea) {
            if (rect.contains(absoluteXCss, absoluteYCss)) return false;
        }

        for (RectF rect : mTouchableArea) {
            if (rect.contains(absoluteXCss, absoluteYCss)) return true;
        }
        return false;
    }

    /** Gets the current height of the bottom bar. */
    private int getBottomBarHeight() {
        return (int) (mFullscreenManager.getBottomControlsHeight()
                - mFullscreenManager.getBottomControlOffset());
    }
}
