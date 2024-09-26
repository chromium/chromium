// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone.stack;

import android.content.Context;
import android.hardware.SensorManager;
import android.util.Log;
import android.view.ViewConfiguration;

import org.chromium.base.MathUtils;

/**
 * This class is vastly copied from {@link android.widget.OverScroller} but decouples the time
 * from the app time so it can be specified manually.
 */
public class StackScroller {
    private int mMode;

    private final SplineStackScroller mScrollerX;
    private final SplineStackScroller mScrollerY;

    private final boolean mFlywheel;

    private static final int SCROLL_MODE = 0;
    private static final int FLING_MODE = 1;

    private static float sViscousFluidScale;
    private static float sViscousFluidNormalize;

    /** Creates an StackScroller with a viscous fluid scroll interpolator and flywheel. */
    public StackScroller(Context context) {
        mFlywheel = true;
        mScrollerX = new SplineStackScroller(context);
        mScrollerY = new SplineStackScroller(context);
        initContants();
    }

    private static void initContants() {
        // This controls the viscous fluid effect (how much of it)
        sViscousFluidScale = 8.0f;
        // must be set to 1.0 (used in viscousFluid())
        sViscousFluidNormalize = 1.0f;
        sViscousFluidNormalize = 1.0f / viscousFluid(1.0f);
    }

    public final void setFrictionMultiplier(float frictionMultiplier) {
        mScrollerX.setFrictionMultiplier(frictionMultiplier);
        mScrollerY.setFrictionMultiplier(frictionMultiplier);
    }

    public final void setXSnapDistance(int snapDistance) {
        mScrollerX.setSnapDistance(snapDistance);
    }

    public final void setYSnapDistance(int snapDistance) {
        mScrollerY.setSnapDistance(snapDistance);
    }

    /**
     * This method should be called when a touch down event is received if snapping is enabled in
     * the X direction.
     *
     * @param index What multiple of the snap distance (i.e. it can be multiplied by the snap
     *              distance) we were closest to when a touch down event was received.
     */
    public final void setCenteredXSnapIndexAtTouchDown(int index) {
        mScrollerX.setCenteredSnapIndexAtTouchDown(index);
    }

    /**
     * This method should be called when a touch down event is received if snapping is enabled in
     * the Y direction.
     *
     * @param index What multiple of the snap distance (i.e. it can be multiplied by the snap
     *              distance) we were closest to when a touch down event was received.
     */
    public final void setCenteredYSnapIndexAtTouchDown(int index) {
        mScrollerY.setCenteredSnapIndexAtTouchDown(index);
    }

    /**
     *
     * Returns whether the scroller has finished scrolling.
     *
     * @return True if the scroller has finished scrolling, false otherwise.
     */
    public final boolean isFinished() {
        return mScrollerX.mFinished && mScrollerY.mFinished;
    }

    /**
     * Force the finished field to a particular value. Contrary to
     * {@link #abortAnimation()}, forcing the animation to finished
     * does NOT cause the scroller to move to the final x and y
     * position.
     *
     * @param finished The new finished value.
     */
    public final void forceFinished(boolean finished) {
        mScrollerX.mFinished = mScrollerY.mFinished = finished;
    }

    /**
     * Returns the current X offset in the scroll.
     *
     * @return The new X offset as an absolute distance from the origin.
     */
    public final int getCurrX() {
        return mScrollerX.mCurrentPosition;
    }

    /**
     * Returns the current Y offset in the scroll.
     *
     * @return The new Y offset as an absolute distance from the origin.
     */
    public final int getCurrY() {
        return mScrollerY.mCurrentPosition;
    }

    /**
     * Returns where the scroll will end. Valid only for "fling" scrolls.
     *
     * @return The final X offset as an absolute distance from the origin.
     */
    public final int getFinalX() {
        return mScrollerX.mFinal;
    }

    /**
     * Returns where the scroll will end. Valid only for "fling" scrolls.
     *
     * @return The final Y offset as an absolute distance from the origin.
     */
    public final int getFinalY() {
        return mScrollerY.mFinal;
    }

    /**
     * Sets where the scroll will end.  Valid only for "fling" scrolls.
     *
     * @param x The final X offset as an absolute distance from the origin.
     */
    public final void setFinalX(int x) {
        mScrollerX.setFinalPosition(x);
    }

    private static float viscousFluid(float x) {
        x *= sViscousFluidScale;
        if (x < 1.0f) {
            x -= (1.0f - (float) Math.exp(-x));
        } else {
            float start = 0.36787945f; // 1/e == exp(-1)
            x = 1.0f - (float) Math.exp(1.0f - x);
            x = start + x * (1.0f - start);
        }
        x *= sViscousFluidNormalize;
        return x;
    }

    /**
     * Call this when you want to know the new location. If it returns true, the
     * animation is not yet finished.
     */
    public boolean computeScrollOffset(long time) {
        if (isFinished()) {
            return false;
        }

        switch (mMode) {
            case SCROLL_MODE:
                // Any scroller can be used for time, since they were started
                // together in scroll mode. We use X here.
                final long elapsedTime = time - mScrollerX.mStartTime;

                final int duration = mScrollerX.mDuration;
                if (elapsedTime < duration) {
                    float q = (float) elapsedTime / duration;
                    q = viscousFluid(q);
                    mScrollerX.updateScroll(q);
                    mScrollerY.updateScroll(q);
                } else {
                    abortAnimation();
                }
                break;

            case FLING_MODE:
                if (!mScrollerX.mFinished) {
                    if (!mScrollerX.update(time)) {
                        if (!mScrollerX.continueWhenFinished(time)) {
                            mScrollerX.finish();
                        }
                    }
                }

                if (!mScrollerY.mFinished) {
                    if (!mScrollerY.update(time)) {
                        if (!mScrollerY.continueWhenFinished(time)) {
                            mScrollerY.finish();
                        }
                    }
                }

                break;

            default:
                break;
        }

        return true;
    }

    /**
     * Start scrolling by providing a starting point and the distance to travel.
     *
     * @param startX Starting horizontal scroll offset in pixels. Positive
     *        numbers will scroll the content to the left.
     * @param startY Starting vertical scroll offset in pixels. Positive numbers
     *        will scroll the content up.
     * @param dx Horizontal distance to travel. Positive numbers will scroll the
     *        content to the left.
     * @param dy Vertical distance to travel. Positive numbers will scroll the
     *        content up.
     * @param duration Duration of the scroll in milliseconds.
     */
    public void startScroll(int startX, int startY, int dx, int dy, long startTime, int duration) {
        mMode = SCROLL_MODE;
        mScrollerX.startScroll(startX, dx, startTime, duration);
        mScrollerY.startScroll(startY, dy, startTime, duration);
    }

    /**
     * Call this when you want to 'spring back' into a valid coordinate range.
     *
     * @param startX Starting X coordinate
     * @param startY Starting Y coordinate
     * @param minX Minimum valid X value
     * @param maxX Maximum valid X value
     * @param minY Minimum valid Y value
     * @param maxY Minimum valid Y value
     * @return true if a springback was initiated, false if startX and startY were
     *          already within the valid range.
     */
    public boolean springBack(
            int startX, int startY, int minX, int maxX, int minY, int maxY, long time) {
        mMode = FLING_MODE;

        // Make sure both methods are called.
        final boolean spingbackX = mScrollerX.springback(startX, minX, maxX, time);
        final boolean spingbackY = mScrollerY.springback(startY, minY, maxY, time);
        return spingbackX || spingbackY;
    }

    /**
     * Start scrolling based on a fling gesture. The distance traveled will
     * depend on the initial velocity of the fling.
     *
     * @param startX Starting point of the scroll (X)
     * @param startY Starting point of the scroll (Y)
     * @param velocityX Initial velocity of the fling (X) measured in pixels per second.
     * @param velocityY Initial velocity of the fling (Y) measured in pixels per second
     * @param minX Minimum X value. The scroller will not scroll past this point
     *            unless overX > 0. If overfling is allowed, it will use minX as
     *            a springback boundary.
     * @param maxX Maximum X value. The scroller will not scroll past this point
     *            unless overX > 0. If overfling is allowed, it will use maxX as
     *            a springback boundary.
     * @param minY Minimum Y value. The scroller will not scroll past this point
     *            unless overY > 0. If overfling is allowed, it will use minY as
     *            a springback boundary.
     * @param maxY Maximum Y value. The scroller will not scroll past this point
     *            unless overY > 0. If overfling is allowed, it will use maxY as
     *            a springback boundary.
     * @param overX Overfling range. If > 0, horizontal overfling in either
     *            direction will be possible.
     * @param overY Overfling range. If > 0, vertical overfling in either
     *            direction will be possible.
     */
    public void fling(
            int startX,
            int startY,
            int velocityX,
            int velocityY,
            int minX,
            int maxX,
            int minY,
            int maxY,
            int overX,
            int overY,
            long time) {
        // Continue a scroll or fling in progress
        if (mFlywheel && !isFinished()) {
            float oldVelocityX = mScrollerX.mCurrVelocity;
            float oldVelocityY = mScrollerY.mCurrVelocity;
            if (Math.signum(velocityX) == Math.signum(oldVelocityX)
                    && Math.signum(velocityY) == Math.signum(oldVelocityY)) {
                velocityX = (int) (velocityX + oldVelocityX);
                velocityY = (int) (velocityY + oldVelocityY);
            }
        }

        mMode = FLING_MODE;
        mScrollerX.fling(startX, velocityX, minX, maxX, overX, time);
        mScrollerY.fling(startY, velocityY, minY, maxY, overY, time);
    }

    /**
     * Tells the X scroller to animate a fling to the specified position.
     *
     * @param startX The initial position for the animation.
     * @param finalX The end position for the animation.
     * @param time The start time to use for the animation.
     */
    public void flingXTo(int startX, int finalX, long time) {
        mScrollerX.flingTo(startX, finalX, time);
    }

    /**
     * Tells the Y scroller to animate a fling to the specified position.
     *
     * @param startY The initial position for the animation.
     * @param finalY The end position for the animation.
     * @param time The start time to use for the animation.
     */
    public void flingYTo(int startY, int finalY, long time) {
        mScrollerY.flingTo(startY, finalY, time);
    }

    /**
     * Stops the animation. Contrary to {@link #forceFinished(boolean)},
     * aborting the animating causes the scroller to move to the final x and y
     * positions.
     *
     * @see #forceFinished(boolean)
     */
    public void abortAnimation() {
        mScrollerX.finish();
        mScrollerY.finish();
    }

    static class SplineStackScroller {
        // Initial position
        private int mStart;

        // Current position
        private int mCurrentPosition;

        // Final position
        private int mFinal;

        // Initial velocity
        private int mVelocity;

        // Current velocity
        private float mCurrVelocity;

        // Constant current deceleration
        private float mDeceleration;

        // Animation starting time, in system milliseconds
        private long mStartTime;

        // Animation duration, in milliseconds
        private int mDuration;

        // Duration to complete spline component of animation
        private int mSplineDuration;

        // Distance to travel along spline animation
        private int mSplineDistance;

        // Whether the animation is currently in progress
        private boolean mFinished;

        // The allowed overshot distance before boundary is reached.
        private int mOver;

        // Fling friction
        private final float mFlingFriction = ViewConfiguration.getScrollFriction();
        private float mFrictionMultiplier = 1.f;

        private int mCenteredSnapIndexAtTouchDown;
        private long mLastMaxFlingTime;

        // If this is non-zero, we enable logic to force the ending scroll position to be an integer
        // multiple of this number.
        private int mSnapDistance;

        // Current state of the animation.
        private int mState = SPLINE;

        // Constant gravity value, used in the deceleration phase.
        private static final float GRAVITY = 2000.0f;

        // A context-specific coefficient adjusted to physical values.
        private final float mPhysicalCoeff;

        private static final float DECELERATION_RATE = (float) (Math.log(0.78) / Math.log(0.9));

        // Keep these in sync with the values in //tools/android/ui/generate_spline_constants.py
        private static final float INFLEXION = 0.35f; // Tension lines cross at (INFLEXION, 1)
        private static final int NB_SAMPLES = 100;

        // Values pregenerated by //tools/android/ui/generate_spline_constants.py
        private static final float[] SPLINE_POSITION = {
            0.000023f, 0.028561f, 0.057052f, 0.085389f, 0.113496f, 0.141299f, 0.168772f, 0.195811f,
            0.222396f, 0.248438f, 0.274002f, 0.298968f, 0.323332f, 0.347096f, 0.370225f, 0.392725f,
            0.414570f, 0.435829f, 0.456419f, 0.476410f, 0.495756f, 0.514549f, 0.532721f, 0.550285f,
            0.567327f, 0.583811f, 0.599748f, 0.615194f, 0.630117f, 0.644548f, 0.658520f, 0.672040f,
            0.685100f, 0.697728f, 0.709951f, 0.721775f, 0.733178f, 0.744231f, 0.754909f, 0.765247f,
            0.775225f, 0.784877f, 0.794206f, 0.803230f, 0.811943f, 0.820371f, 0.828519f, 0.836379f,
            0.843977f, 0.851323f, 0.858411f, 0.865253f, 0.871853f, 0.878233f, 0.884389f, 0.890316f,
            0.896047f, 0.901557f, 0.906874f, 0.911995f, 0.916932f, 0.921675f, 0.926242f, 0.930633f,
            0.934848f, 0.938901f, 0.942790f, 0.946522f, 0.950094f, 0.953518f, 0.956790f, 0.959924f,
            0.962913f, 0.965762f, 0.968482f, 0.971068f, 0.973523f, 0.975851f, 0.978060f, 0.980149f,
            0.982115f, 0.983968f, 0.985709f, 0.987335f, 0.988855f, 0.990269f, 0.991577f, 0.992784f,
            0.993891f, 0.994899f, 0.995811f, 0.996627f, 0.997352f, 0.997985f, 0.998529f, 0.998984f,
            0.999354f, 0.999639f, 0.999840f, 0.999960f, 1.000000f
        };
        private static final float[] SPLINE_TIME = {
            0.000002f, 0.003501f, 0.007003f, 0.010507f, 0.014014f, 0.017523f, 0.021044f, 0.024569f,
            0.028098f, 0.031640f, 0.035195f, 0.038755f, 0.042337f, 0.045926f, 0.049530f, 0.053156f,
            0.056798f, 0.060456f, 0.064138f, 0.067844f, 0.071568f, 0.075316f, 0.079097f, 0.082904f,
            0.086737f, 0.090596f, 0.094489f, 0.098416f, 0.102385f, 0.106382f, 0.110422f, 0.114497f,
            0.118615f, 0.122783f, 0.126987f, 0.131243f, 0.135549f, 0.139900f, 0.144309f, 0.148776f,
            0.153296f, 0.157881f, 0.162519f, 0.167230f, 0.172007f, 0.176851f, 0.181767f, 0.186757f,
            0.191835f, 0.196993f, 0.202230f, 0.207555f, 0.212973f, 0.218491f, 0.224109f, 0.229833f,
            0.235656f, 0.241598f, 0.247659f, 0.253837f, 0.260147f, 0.266598f, 0.273178f, 0.279912f,
            0.286812f, 0.293848f, 0.301075f, 0.308475f, 0.316060f, 0.323840f, 0.331824f, 0.340037f,
            0.348487f, 0.357182f, 0.366129f, 0.375349f, 0.384886f, 0.394732f, 0.404901f, 0.415447f,
            0.426381f, 0.437738f, 0.449557f, 0.461860f, 0.474729f, 0.488177f, 0.502311f, 0.517150f,
            0.532822f, 0.549455f, 0.567130f, 0.586069f, 0.606443f, 0.628536f, 0.652774f, 0.679739f,
            0.710244f, 0.745801f, 0.789246f, 0.848082f, 1.000000f
        };

        private static final int SPLINE = 0;
        private static final int CUBIC = 1;
        private static final int BALLISTIC = 2;

        // The following parameters are only used when snapping is enabled (mSnapDistance != 0).

        // Maximum number of snapped positions to scroll over for a call to fling().
        private static final int MAX_SNAP_SCROLL = 12;

        // Minimum fling velocity to scroll away from the currently-snapped position..
        private static final int SINGLE_SNAP_MIN_VELOCITY = 100;
        // Minimum fling velocity to scroll two snap postions instead of one.
        private static final int DOUBLE_SNAP_MIN_VELOCITY = 1800;
        // Minimum fling velocity to scroll three snap positions instead of one.
        private static final int TRIPLE_SNAP_MIN_VELOCITY = 2500;
        // Minimum fling velocity to scroll by MAX_SNAP_SCROLL positions.
        private static final int MAX_SNAP_SCROLL_MIN_VELOCITY = 5000;

        // If we receive a fling within this many milliseconds of receiving a previous fling that
        // caused us to do a maximum distance scroll (and a few other validity checks hold), we
        // lower the velocity threshold for the new fling to also do a maximum velocity scroll;
        private static final int REPEATED_FLING_TIMEOUT = 1500;
        // Minimum velocity for a "repeated fling" (see previous comment) to trigger a maximum
        // velocity scroll;
        private static final int REPEATED_FLING_VELOCITY_THRESHOLD = 1000;

        SplineStackScroller(Context context) {
            mFinished = true;
            final float ppi = context.getResources().getDisplayMetrics().density * 160.0f;
            mPhysicalCoeff =
                    SensorManager.GRAVITY_EARTH // g (m/s^2)
                            * 39.37f // inch/meter
                            * ppi
                            * 0.84f; // look and feel tuning
        }

        void setFrictionMultiplier(float frictionMultiplier) {
            mFrictionMultiplier = frictionMultiplier;
        }

        private float getFriction() {
            return mFlingFriction * mFrictionMultiplier;
        }

        void setSnapDistance(int snapDistance) {
            mSnapDistance = snapDistance;
        }

        void setCenteredSnapIndexAtTouchDown(int centeredSnapDistanceAtTouchDown) {
            mCenteredSnapIndexAtTouchDown = centeredSnapDistanceAtTouchDown;
        }

        void updateScroll(float q) {
            mCurrentPosition = mStart + Math.round(q * (mFinal - mStart));
        }

        /*
         * Get a signed deceleration that will reduce the velocity.
         */
        private static float getDeceleration(int velocity) {
            return velocity > 0 ? -GRAVITY : GRAVITY;
        }

        /*
         * Modifies mDuration to the duration it takes to get from start to newFinal using the
         * spline interpolation. The previous duration was needed to get to oldFinal.
         */
        private void adjustDuration(int start, int oldFinal, int newFinal) {
            final int oldDistance = oldFinal - start;
            final int newDistance = newFinal - start;
            final float x = Math.abs((float) newDistance / oldDistance);
            final int index = (int) (NB_SAMPLES * x);
            if (index < NB_SAMPLES) {
                final float xInf = (float) index / NB_SAMPLES;
                final float xSup = (float) (index + 1) / NB_SAMPLES;
                final float tInf = SPLINE_TIME[index];
                final float tSup = SPLINE_TIME[index + 1];
                final float timeCoef = tInf + (x - xInf) / (xSup - xInf) * (tSup - tInf);
                mDuration = (int) (mDuration * timeCoef);
            }
        }

        void startScroll(int start, int distance, long startTime, int duration) {
            mFinished = false;

            mStart = start;
            mFinal = start + distance;

            mStartTime = startTime;
            mDuration = duration;

            // Unused
            mDeceleration = 0.0f;
            mVelocity = 0;
        }

        void finish() {
            mCurrentPosition = mFinal;
            // Not reset since WebView relies on this value for fast fling.
            // TODO: restore when WebView uses the fast fling implemented in this class.
            // mCurrVelocity = 0.0f;
            mFinished = true;
        }

        void setFinalPosition(int position) {
            mFinal = position;
            mFinished = false;
        }

        boolean springback(int start, int min, int max, long time) {
            mFinished = true;

            mStart = mFinal = start;
            mVelocity = 0;

            mStartTime = time;
            mDuration = 0;

            if (start < min) {
                startSpringback(start, min, 0);
            } else if (start > max) {
                startSpringback(start, max, 0);
            }
            return !mFinished;
        }

        private void startSpringback(int start, int end, int velocity) {
            // mStartTime has been set
            mFinished = false;
            mState = CUBIC;
            mStart = start;
            mFinal = end;
            final int delta = start - end;
            mDeceleration = getDeceleration(delta);
            // TODO take velocity into account
            mVelocity = -delta; // only sign is used
            mOver = Math.abs(delta);
            mDuration = (int) (1000.0 * Math.sqrt(-2.0 * delta / mDeceleration));
        }

        int computeSnapScrollDistance(int velocity) {
            if (Math.abs(velocity) < SINGLE_SNAP_MIN_VELOCITY) return 0;
            if (Math.abs(velocity) < DOUBLE_SNAP_MIN_VELOCITY) return 1;
            if (Math.abs(velocity) < TRIPLE_SNAP_MIN_VELOCITY) return 2;
            if (Math.abs(velocity) >= MAX_SNAP_SCROLL_MIN_VELOCITY) return MAX_SNAP_SCROLL;

            // For fling velocities between TRIPLE_SNAP_MIN_VELOCITY and
            // MAX_SNAP_SCROLL_MIN_VELOCITY, we do linear interpolation to decide how many snap
            // positions to scroll by.
            float increment =
                    (MAX_SNAP_SCROLL_MIN_VELOCITY - TRIPLE_SNAP_MIN_VELOCITY)
                            / ((float) (MAX_SNAP_SCROLL - 3));
            return (int) ((Math.abs(velocity) - TRIPLE_SNAP_MIN_VELOCITY) / increment) + 3;
        }

        void fling(int start, int velocity, int min, int max, int over, long time) {
            if (mSnapDistance != 0) {
                doSnapScroll(start, velocity, min, max, time);
                return;
            }

            mOver = over;
            mFinished = false;
            mCurrVelocity = mVelocity = velocity;
            mDuration = mSplineDuration = 0;
            mStartTime = time;
            mCurrentPosition = mStart = start;

            if (start > max || start < min) {
                startAfterEdge(start, min, max, velocity, time);
                return;
            }

            mState = SPLINE;
            double totalDistance = 0.0;

            if (velocity != 0) {
                mDuration = mSplineDuration = getSplineFlingDuration(velocity);
                totalDistance = getSplineFlingDistance(velocity);
            }

            mSplineDistance = (int) (totalDistance * Math.signum(velocity));
            mFinal = start + mSplineDistance;

            // Clamp to a valid final position
            if (mFinal < min) {
                adjustDuration(mStart, mFinal, min);
                mFinal = min;
            }

            if (mFinal > max) {
                adjustDuration(mStart, mFinal, max);
                mFinal = max;
            }
        }

        private void doSnapScroll(int start, int velocity, int min, int max, long time) {
            boolean sameDirection = (Math.signum(velocity) == Math.signum(mCurrVelocity));

            int numTabsToScroll = computeSnapScrollDistance(velocity);
            if (numTabsToScroll == MAX_SNAP_SCROLL
                    || (time < mLastMaxFlingTime + REPEATED_FLING_TIMEOUT
                            && sameDirection
                            && Math.abs(velocity) > REPEATED_FLING_VELOCITY_THRESHOLD)) {
                // After receiving one "max speed" fling, give a boost to subsequent flings to make
                // it easier to scroll by a large number of tabs.
                mLastMaxFlingTime = time;
                numTabsToScroll = MAX_SNAP_SCROLL;
            }

            int newCenteredTab =
                    mCenteredSnapIndexAtTouchDown - (int) Math.signum(velocity) * numTabsToScroll;
            double newPositionPostSnapping = -newCenteredTab * mSnapDistance;

            double newPositionPostClamping =
                    MathUtils.clamp((float) newPositionPostSnapping, min, max);
            if (newPositionPostSnapping == mCurrentPosition) {
                // Don't apply the repeated fling boost right after a fling that didn't actually
                // scroll anything.
                mLastMaxFlingTime = 0;
                return;
            }

            flingTo(start, (int) newPositionPostSnapping, time);
        }

        /**
         * Animates a fling to the specified position.
         *
         * @param startPosition The initial position for the animation.
         * @param finalPosition The end position for the animation.
         * @param time The start time to use for the animation.
         */
        void flingTo(int startPosition, int finalPosition, long time) {
            mCurrentPosition = mStart = startPosition;
            mFinal = finalPosition;
            mStartTime = time;
            mSplineDistance = finalPosition - startPosition;
            mFinished = false;
            mOver = 0;
            mState = SPLINE;

            mCurrVelocity =
                    (int)
                            (Math.signum(mSplineDistance)
                                    * getSplineFlingDistanceInverse(Math.abs(mSplineDistance)));
            mDuration = mSplineDuration = getSplineFlingDuration((int) mCurrVelocity);
        }

        private double getSplineDeceleration(int velocity) {
            return Math.log(INFLEXION * Math.abs(velocity) / (getFriction() * mPhysicalCoeff));
        }

        // Note: this always returns a positive velocity. The desired velocity may be negative (with
        // the same magnitude).
        private int getSplineDecelerationInverse(double deceleration) {
            return (int)
                    Math.round(
                            Math.exp(deceleration) * (getFriction() * mPhysicalCoeff) / INFLEXION);
        }

        private double getSplineFlingDistance(int velocity) {
            final double l = getSplineDeceleration(velocity);
            final double decelMinusOne = DECELERATION_RATE - 1.0;
            return getFriction() * mPhysicalCoeff * Math.exp(DECELERATION_RATE / decelMinusOne * l);
        }

        /* Returns the duration, expressed in milliseconds */
        private int getSplineFlingDuration(int velocity) {
            final double l = getSplineDeceleration(velocity);
            final double decelMinusOne = DECELERATION_RATE - 1.0;
            return (int) (1000.0 * Math.exp(l / decelMinusOne));
        }

        // This lets us get the required fling velocity to make the scroller move a certain
        // distance. We use this to implement snapping by calculating where a fling of a given
        // velocity would move the scroller to, rounding to the nearest multiple of the current snap
        // distance, and inverting to get the final velocity to use (close enough to the initial
        // velocity that it's really noticeable that we changed it).
        private int getSplineFlingDistanceInverse(double distance) {
            double decelMinusOne = DECELERATION_RATE - 1.0;
            double splineDeceleration =
                    Math.log(distance / (getFriction() * mPhysicalCoeff))
                            * decelMinusOne
                            / DECELERATION_RATE;
            return getSplineDecelerationInverse(splineDeceleration);
        }

        private void fitOnBounceCurve(int start, int end, int velocity) {
            // Simulate a bounce that started from edge
            final float durationToApex = -velocity / mDeceleration;
            final float distanceToApex = velocity * velocity / 2.0f / Math.abs(mDeceleration);
            final float distanceToEdge = Math.abs(end - start);
            final float totalDuration =
                    (float)
                            Math.sqrt(
                                    2.0
                                            * (distanceToApex + distanceToEdge)
                                            / Math.abs(mDeceleration));
            mStartTime -= (int) (1000.0f * (totalDuration - durationToApex));
            mStart = end;
            mVelocity = (int) (-mDeceleration * totalDuration);
        }

        private void startBounceAfterEdge(int start, int end, int velocity) {
            mDeceleration = getDeceleration(velocity == 0 ? start - end : velocity);
            fitOnBounceCurve(start, end, velocity);
            onEdgeReached();
        }

        private void startAfterEdge(int start, int min, int max, int velocity, long time) {
            if (start > min && start < max) {
                Log.e("StackScroller", "startAfterEdge called from a valid position");
                mFinished = true;
                return;
            }
            final boolean positive = start > max;
            final int edge = positive ? max : min;
            final int overDistance = start - edge;
            boolean keepIncreasing = overDistance * velocity >= 0;
            if (keepIncreasing) {
                // Will result in a bounce or a to_boundary depending on velocity.
                startBounceAfterEdge(start, edge, velocity);
            } else {
                final double totalDistance = getSplineFlingDistance(velocity);
                if (totalDistance > Math.abs(overDistance)) {
                    fling(
                            start,
                            velocity,
                            positive ? min : start,
                            positive ? start : max,
                            mOver,
                            time);
                } else {
                    startSpringback(start, edge, velocity);
                }
            }
        }

        private void onEdgeReached() {
            // mStart, mVelocity and mStartTime were adjusted to their values when edge was reached.
            float distance = mVelocity * mVelocity / (2.0f * Math.abs(mDeceleration));
            final float sign = Math.signum(mVelocity);

            if (distance > mOver) {
                // Default deceleration is not sufficient to slow us down before boundary
                mDeceleration = -sign * mVelocity * mVelocity / (2.0f * mOver);
                distance = mOver;
            }

            mOver = (int) distance;
            mState = BALLISTIC;
            mFinal = mStart + (int) (mVelocity > 0 ? distance : -distance);
            mDuration = -(int) (1000.0f * mVelocity / mDeceleration);
        }

        boolean continueWhenFinished(long time) {
            switch (mState) {
                case SPLINE:
                    // Duration from start to null velocity
                    if (mDuration < mSplineDuration) {
                        // If the animation was clamped, we reached the edge
                        mStart = mFinal;
                        // TODO Better compute speed when edge was reached
                        mVelocity = (int) mCurrVelocity;
                        mDeceleration = getDeceleration(mVelocity);
                        mStartTime += mDuration;
                        onEdgeReached();
                    } else {
                        // Normal stop, no need to continue
                        return false;
                    }
                    break;
                case BALLISTIC:
                    mStartTime += mDuration;
                    startSpringback(mFinal, mStart, 0);
                    break;
                case CUBIC:
                    return false;
            }

            update(time);
            return true;
        }

        /*
         * Update the current position and velocity for current time. Returns
         * true if update has been done and false if animation duration has been
         * reached.
         */
        boolean update(long time) {
            final long currentTime = time - mStartTime;

            if (currentTime > mDuration) {
                return false;
            }

            double distance = 0.0;
            switch (mState) {
                case SPLINE:
                    {
                        final float t = (float) currentTime / mSplineDuration;
                        final int index = (int) (NB_SAMPLES * t);
                        float distanceCoef = 1.f;
                        float velocityCoef = 0.f;
                        if (index < NB_SAMPLES) {
                            final float tInf = (float) index / NB_SAMPLES;
                            final float tSup = (float) (index + 1) / NB_SAMPLES;
                            final float dInf = SPLINE_POSITION[index];
                            final float dSup = SPLINE_POSITION[index + 1];
                            velocityCoef = (dSup - dInf) / (tSup - tInf);
                            distanceCoef = dInf + (t - tInf) * velocityCoef;
                        }

                        distance = distanceCoef * mSplineDistance;
                        mCurrVelocity = velocityCoef * mSplineDistance / mSplineDuration * 1000.0f;
                        break;
                    }

                case BALLISTIC:
                    {
                        final float t = currentTime / 1000.0f;
                        mCurrVelocity = mVelocity + mDeceleration * t;
                        distance = mVelocity * t + mDeceleration * t * t / 2.0f;
                        break;
                    }

                case CUBIC:
                    {
                        final float t = (float) currentTime / mDuration;
                        final float t2 = t * t;
                        final float sign = Math.signum(mVelocity);
                        distance = sign * mOver * (3.0f * t2 - 2.0f * t * t2);
                        mCurrVelocity = sign * mOver * 6.0f * (-t + t2);
                        break;
                    }
            }

            mCurrentPosition = mStart + (int) Math.round(distance);

            return true;
        }
    }
}
