// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import android.content.res.Configuration;
import android.graphics.Matrix;
import android.graphics.Point;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Arrays;

/**
 * A container for holding the portrait and landscape transformation matrices, and the specific
 * window dimensions for which those matrices are valid.
 *
 * <p>This class handles the serialization logic to string formats for storage in SharedPreferences.
 */
@NullMarked
public class BackgroundImageInfo {
    private static final String TAG = "BackgroundImageInfo";
    private static final String DELIMITER = "|";

    private @Nullable Point mPortraitWindowSize;
    private @Nullable Point mLandscapeWindowSize;

    private Matrix mPortraitMatrix;
    private Matrix mLandscapeMatrix;

    public BackgroundImageInfo(
            Matrix portrait,
            Matrix landscape,
            @Nullable Point portraitWindowSize,
            @Nullable Point landscapeWindowSize) {
        mPortraitMatrix = portrait;
        mLandscapeMatrix = landscape;
        mPortraitWindowSize = portraitWindowSize;
        mLandscapeWindowSize = landscapeWindowSize;
    }

    /**
     * Static Factory Method to create an instance from SharedPreferences strings.
     *
     * @param portraitInfoStr The serialized string for portrait (Matrix | Width | Height).
     * @param landscapeInfoStr The serialized string for landscape (Matrix | Width | Height).
     * @return A populated BackgroundImageInfo object, or null if parsing failed.
     */
    public static @Nullable BackgroundImageInfo createFromStrings(
            @Nullable String portraitInfoStr, @Nullable String landscapeInfoStr) {
        if (TextUtils.isEmpty(portraitInfoStr) || TextUtils.isEmpty(landscapeInfoStr)) {
            return null;
        }

        // 1. Parses portrait information
        // Format: "MatrixString" OR "MatrixString|Width|Height"
        // Note: split takes a regex, so we must escape the pipe symbol.
        String[] portraitParts = portraitInfoStr.split("\\" + DELIMITER);
        Matrix portraitMatrix = null;
        Point portraitSize = null;

        if (portraitParts.length > 0) {
            portraitMatrix = stringToMatrix(portraitParts[0]);
            if (portraitParts.length >= 3) {
                portraitSize = parseWindowSize(portraitParts[1], portraitParts[2]);
            }
        }

        if (portraitMatrix == null) {
            return null;
        }

        // 2. Parses landscape information
        String[] landscapeParts = landscapeInfoStr.split("\\" + DELIMITER);
        Matrix landscapeMatrix = null;
        Point landscapeSize = null;

        if (landscapeParts.length > 0) {
            landscapeMatrix = stringToMatrix(landscapeParts[0]);
            if (landscapeParts.length >= 3) {
                landscapeSize = parseWindowSize(landscapeParts[1], landscapeParts[2]);
            }
        }

        if (landscapeMatrix == null) {
            return null;
        }

        return new BackgroundImageInfo(
                portraitMatrix, landscapeMatrix, portraitSize, landscapeSize);
    }

    /**
     * Serializes the portrait matrix and its window size (if available). Format: "MatrixString" or
     * "MatrixString|Width|Height"
     */
    public String getPortraitInfoString() {
        return getInfoString(mPortraitMatrix, mPortraitWindowSize);
    }

    /**
     * Serializes the landscape matrix and its window size (if available). Format: "MatrixString" or
     * "MatrixString|Width|Height"
     */
    public String getLandscapeInfoString() {
        return getInfoString(mLandscapeMatrix, mLandscapeWindowSize);
    }

    private String getInfoString(Matrix matrix, @Nullable Point windowSize) {
        StringBuilder builder = new StringBuilder(matrixToString(matrix));
        if (windowSize != null) {
            builder.append(DELIMITER).append(windowSize.x).append(DELIMITER).append(windowSize.y);
        }
        return builder.toString();
    }

    /**
     * Returns the cached window size for a specific orientation.
     *
     * @param orientation {@link Configuration#ORIENTATION_PORTRAIT} or {@link
     *     Configuration#ORIENTATION_LANDSCAPE}.
     * @return The cached size, or null if not yet calculated/saved for that orientation.
     */
    public @Nullable Point getWindowSize(int orientation) {
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            return mPortraitWindowSize;
        }
        return mLandscapeWindowSize;
    }

    @VisibleForTesting
    public @Nullable Point getPortraitWindowSize() {
        return getWindowSize(Configuration.ORIENTATION_PORTRAIT);
    }

    @VisibleForTesting
    public @Nullable Point getLandscapeWindowSize() {
        return getWindowSize(Configuration.ORIENTATION_LANDSCAPE);
    }

    /**
     * Sets the window size for the specified orientation.
     *
     * @param orientation {@link Configuration#ORIENTATION_PORTRAIT} or {@link
     *     Configuration#ORIENTATION_LANDSCAPE}.
     * @param windowSize The {@link Point} representing the width and height of the window.
     */
    public void setWindowSize(int orientation, Point windowSize) {
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            mPortraitWindowSize = windowSize;
            return;
        }
        mLandscapeWindowSize = windowSize;
    }

    /**
     * Returns the transformation matrix for the specified orientation.
     *
     * @param orientation The orientation to retrieve the matrix for (e.g. {@link
     *     Configuration#ORIENTATION_PORTRAIT}).
     * @return The {@link Matrix} associated with the given orientation.
     */
    public Matrix getMatrix(int orientation) {
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            return mPortraitMatrix;
        }
        return mLandscapeMatrix;
    }

    @VisibleForTesting
    public Matrix getPortraitMatrix() {
        return getMatrix(Configuration.ORIENTATION_PORTRAIT);
    }

    @VisibleForTesting
    public Matrix getLandscapeMatrix() {
        return getMatrix(Configuration.ORIENTATION_LANDSCAPE);
    }

    /**
     * Sets the transformation matrix for the specified orientation.
     *
     * @param orientation {@link Configuration#ORIENTATION_PORTRAIT} or {@link
     *     Configuration#ORIENTATION_LANDSCAPE}.
     * @param matrix The {@link Matrix} to associate with the given orientation.
     */
    public void setMatrix(int orientation, Matrix matrix) {
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            mPortraitMatrix = matrix;
            return;
        }
        mLandscapeMatrix = matrix;
    }

    /**
     * Returns the string representations of width and height into a valid Point object.
     *
     * @param widthString The width as a string.
     * @param heightString The height as a string.
     * @return A {@link Point} containing the parsed dimensions if valid (positive integers), or
     *     null if parsing fails.
     */
    static @Nullable Point parseWindowSize(String widthString, String heightString) {
        try {
            int width = Integer.parseInt(widthString);
            int height = Integer.parseInt(heightString);
            if (width > 0 && height > 0) {
                return new Point(width, height);
            }
        } catch (NumberFormatException e) {
            Log.i(TAG, "Failed to parse window dimensions", e);
        }
        return null;
    }

    @VisibleForTesting
    public static String matrixToString(Matrix matrix) {
        float[] values = new float[9];
        matrix.getValues(values);
        return Arrays.toString(values);
    }

    @VisibleForTesting
    static @Nullable Matrix stringToMatrix(String matrixString) {
        if (matrixString == null || !matrixString.startsWith("[") || !matrixString.endsWith("]")) {
            return null;
        }
        try {
            // Remove brackets and spaces
            String[] stringValues =
                    matrixString.substring(1, matrixString.length() - 1).split(", ");
            if (stringValues.length != 9) return null;

            float[] values = new float[9];
            for (int i = 0; i < 9; i++) {
                values[i] = Float.parseFloat(stringValues[i]);
            }

            Matrix matrix = new Matrix();
            matrix.setValues(values);
            return matrix;
        } catch (Exception e) {
            Log.i(TAG, "Error in stringToMatrix: " + e);
            return null;
        }
    }

    /**
     * Creates a deep copy of the provided {@link BackgroundImageInfo}.
     *
     * <p>This creates new instances of the {@link Matrix} objects to ensure that modifications to
     * the copy (e.g., during user interaction) do not affect the original source.
     *
     * @param info The {@link BackgroundImageInfo} to copy.
     * @return A new {@link BackgroundImageInfo} instance with independent matrix data, or {@code
     *     null} if the input is null.
     */
    public static @Nullable BackgroundImageInfo getDeepCopy(@Nullable BackgroundImageInfo info) {
        if (info == null) {
            return null;
        }
        return new BackgroundImageInfo(
                new Matrix(info.getPortraitMatrix()),
                new Matrix(info.getLandscapeMatrix()),
                info.getPortraitWindowSize(),
                info.getLandscapeWindowSize());
    }
}
