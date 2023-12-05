// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

/** Contains various math utilities used throughout Chrome Mobile. */
public class MathUtils {
    /** A minimum difference to use when comparing floats for equality. */
    public static final float EPSILON = 0.001f;

    private MathUtils() {}

    /**
     * Returns the passed in value if it resides within the specified range (inclusive).  If not,
     * it will return the closest boundary from the range.  The ordering of the boundary values does
     * not matter.
     *
     * @param value The value to be compared against the range.
     * @param a First boundary range value.
     * @param b Second boundary range value.
     * @return The passed in value if it is within the range, otherwise the closest boundary value.
     */
    public static int clamp(int value, int a, int b) {
        int min = (a > b) ? b : a;
        int max = (a > b) ? a : b;
        if (value < min) {
            value = min;
        } else if (value > max) {
            value = max;
        }
        return value;
    }

    /**
     * Returns the passed in value if it resides within the specified range (inclusive).  If not,
     * it will return the closest boundary from the range.  The ordering of the boundary values does
     * not matter.
     *
     * @param value The value to be compared against the range.
     * @param a First boundary range value.
     * @param b Second boundary range value.
     * @return The passed in value if it is within the range, otherwise the closest boundary value.
     */
    public static long clamp(long value, long a, long b) {
        long min = (a > b) ? b : a;
        long max = (a > b) ? a : b;
        if (value < min) {
            value = min;
        } else if (value > max) {
            value = max;
        }
        return value;
    }

    /**
     * Returns the passed in value if it resides within the specified range (inclusive).  If not,
     * it will return the closest boundary from the range.  The ordering of the boundary values does
     * not matter.
     *
     * @param value The value to be compared against the range.
     * @param a First boundary range value.
     * @param b Second boundary range value.
     * @return The passed in value if it is within the range, otherwise the closest boundary value.
     */
    public static float clamp(float value, float a, float b) {
        float min = (a > b) ? b : a;
        float max = (a > b) ? a : b;
        if (value < min) {
            value = min;
        } else if (value > max) {
            value = max;
        }
        return value;
    }

    /**
     * Computes a%b that is positive. Note that result of % operation is not always positive.
     * @return a%b >= 0 ? a%b : a%b + b
     */
    public static int positiveModulo(int a, int b) {
        int mod = a % b;
        return mod >= 0 ? mod : mod + b;
    }

    /**
     * Moves {@code value} forward to {@code target} based on {@code speed}.
     * @param value  The current value.
     * @param target The target value.
     * @param speed  How far to move {@code value} to {@code target}.  0 doesn't move it at all.  1
     *               moves it to {@code target}.
     * @return       The new interpolated value.
     */
    public static float interpolate(float value, float target, float speed) {
        return (value + (target - value) * speed);
    }

    /**
     * Smooth a value between 0 and 1.
     * @param t The value to smooth.
     * @return  The smoothed value between 0 and 1.
     */
    public static float smoothstep(float t) {
        return t * t * (3.0f - 2.0f * t);
    }

    /**
     * Flips {@code value} iff {@code flipSign} is {@code true}.
     * @param value    The value to flip.
     * @param flipSign Whether or not to flip the value.
     * @return         {@code value} iff {@code flipSign} is {@code false}, otherwise negative
     *                 {@code value}.
     */
    public static int flipSignIf(int value, boolean flipSign) {
        return flipSign ? -value : value;
    }

    /**
     * Flips {@code value} iff {@code flipSign} is {@code true}.
     * @param value    The value to flip.
     * @param flipSign Whether or not to flip the value.
     * @return         {@code value} iff {@code flipSign} is {@code false}, otherwise negative
     *                 {@code value}.
     */
    public static float flipSignIf(float value, boolean flipSign) {
        return flipSign ? -value : value;
    }

    /**
     * Determine if two floats are equal.
     * @param f1 The first float to compare.
     * @param f2 The second float to compare.
     * @return True if the floats are equal.
     */
    public static boolean areFloatsEqual(float f1, float f2) {
        return Math.abs(f1 - f2) < MathUtils.EPSILON;
    }

    /**
     * Compute the distance between two points.
     * @param x1 X of point 1.
     * @param y1 Y of point 1.
     * @param x2 X of point 2.
     * @param y2 Y of point 2.
     * @return The distance between the two points.
     */
    public static float distance(float x1, float y1, float x2, float y2) {
        float xDist = x2 - x1;
        float yDist = y2 - y1;
        return (float) Math.sqrt(xDist * xDist + yDist * yDist);
    }

    /**
     * Compute the distance given two coordinate vectors
     */
    public static float distance(float distanceX, float distanceY) {
        return (float) Math.sqrt(distanceX * distanceX + distanceY * distanceY);
    }

    /**
     * Maps {@code value} in [{@code fromStart}, {@code fromStop}] to
     * [{@code toStart}, {@code toStop}].
     *
     * @param value A number in [{@code fromStart}, {@code fromStop}].
     * @param fromStart Lower range of {@code value}.
     * @param fromStop Upper range of {@code value}.
     * @param toStart Lower range of mapped value.
     * @param toStop Upper range of mapped value.
     * @return mapped value.
     */
    public static float map(
            float value, float fromStart, float fromStop, float toStart, float toStop) {
        return toStart + (toStop - toStart) * ((value - fromStart) / (fromStop - fromStart));
    }

    /**
     * Round the given value to two decimal places.
     *
     * @param value double The value to round.
     * @return double The value rounded to two decimal places.
     */
    public static double roundTwoDecimalPlaces(double value) {
        return (double) Math.round(value * 100) / 100;
    }
}
