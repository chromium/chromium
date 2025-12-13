// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme.upload_image;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import static org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo.matrixToString;
import static org.chromium.chrome.browser.ntp_customization.theme.upload_image.BackgroundImageInfo.stringToMatrix;

import android.graphics.Matrix;
import android.graphics.Point;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link BackgroundImageInfo}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BackgroundImageInfoUnitTest {

    @Test
    public void testMatrixToString_identityMatrix_returnsCorrectString() {
        // Creates an identity matrix.
        Matrix matrix = new Matrix();

        String expected = "[1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]";
        assertEquals(expected, BackgroundImageInfo.matrixToString(matrix));
    }

    @Test
    public void testMatrixToString_scaledMatrix_returnsCorrectString() {
        Matrix matrix = new Matrix();
        matrix.setScale(2.5f, 3.5f);

        // Verifies if the string for a matrix scaled by (2.5, 3.5)
        // Note: Android Matrix order is [ScaleX, SkewX, TransX, SkewY, ScaleY, TransY, ...]
        String expected = "[2.5, 0.0, 0.0, 0.0, 3.5, 0.0, 0.0, 0.0, 1.0]";
        assertEquals(expected, BackgroundImageInfo.matrixToString(matrix));
    }

    @Test
    public void testStringToMatrix_validString_returnsCorrectMatrix() {
        String matrixString = "[1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0]";
        Matrix resultMatrix = stringToMatrix(matrixString);

        assertNotNull(resultMatrix);

        Matrix expectedMatrix = new Matrix();
        expectedMatrix.setValues(new float[] {1f, 2f, 3f, 4f, 5f, 6f, 7f, 8f, 9f});

        // Matrix.equals() checks reference equality, so we compare content via string
        assertEquals(expectedMatrix.toShortString(), resultMatrix.toShortString());
    }

    @Test
    public void testStringToMatrix_nullInput_returnsNull() {
        assertNull(stringToMatrix(null));
    }

    @Test
    public void testStringToMatrix_emptyString_returnsNull() {
        assertNull(stringToMatrix(""));
    }

    @Test
    public void testStringToMatrix_malformedString_noBrackets_returnsNull() {
        assertNull(stringToMatrix("1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0"));
    }

    @Test
    public void testStringToMatrix_wrongNumberOfValues_returnsNull() {
        assertNull(stringToMatrix("[1.0, 2.0, 3.0]"));
    }

    @Test
    public void testStringToMatrix_nonFloatValues_returnsNull() {
        assertNull(stringToMatrix("[1.0, abc, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0]"));
    }

    @Test
    public void testParseWindowSize_ValidInputs() {
        Point result = BackgroundImageInfo.parseWindowSize("1080", "1920");
        assertEquals(1080, result.x);
        assertEquals(1920, result.y);
    }

    @Test
    public void testParseWindowSize_InvalidIntegers() {
        // Non-numeric strings
        assertNull(BackgroundImageInfo.parseWindowSize("abc", "1920"));
        assertNull(BackgroundImageInfo.parseWindowSize("1080", "xyz"));
        assertNull(BackgroundImageInfo.parseWindowSize("10.5", "1920")); // Floats are not ints
    }

    @Test
    public void testParseWindowSize_NonPositiveValues() {
        // Width <= 0
        assertNull(BackgroundImageInfo.parseWindowSize("0", "1920"));
        assertNull(BackgroundImageInfo.parseWindowSize("-100", "1920"));

        // Height <= 0
        assertNull(BackgroundImageInfo.parseWindowSize("1080", "0"));
        assertNull(BackgroundImageInfo.parseWindowSize("1080", "-500"));
    }

    @Test
    public void testParseWindowSize_EmptyInputs() {
        assertNull(BackgroundImageInfo.parseWindowSize("", "1920"));
        assertNull(BackgroundImageInfo.parseWindowSize("1080", ""));
    }

    @Test
    public void testCreateFromStrings_ValidData() {
        String portraitString = "[1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]|1080|1920";
        String landscapeString = "[2.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 15.0]|1920|1080";

        BackgroundImageInfo info =
                BackgroundImageInfo.createFromStrings(portraitString, landscapeString);

        // Verifies image information in portrait
        assertEquals(
                "[1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0]",
                BackgroundImageInfo.matrixToString(info.getPortraitMatrix()));
        Point portraitSize = info.getPortraitWindowSize();
        assertEquals(1080, portraitSize.x);
        assertEquals(1920, portraitSize.y);

        // Verifies image information in landscape
        assertEquals(
                "[2.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 15.0]",
                BackgroundImageInfo.matrixToString(info.getLandscapeMatrix()));
        Point landSize = info.getLandscapeWindowSize();
        assertEquals(1920, landSize.x);
        assertEquals(1080, landSize.y);
    }

    @Test
    public void testCreateFromStrings_NoWindowSize() {
        Matrix matrix = new Matrix();
        String matrixString = matrixToString(matrix);

        // Strings without the |width|height suffix
        BackgroundImageInfo info =
                BackgroundImageInfo.createFromStrings(matrixString, matrixString);

        assertNull(info.getPortraitWindowSize());
        assertNull(info.getLandscapeWindowSize());
    }
}
