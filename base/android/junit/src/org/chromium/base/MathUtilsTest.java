// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static org.chromium.base.MathUtils.EPSILON;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link org.chromium.base.MathUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class MathUtilsTest {
    private static final String ROUND_FAILURE =
            "Failure to correctly round value to two decimal places.";
    private static final String CLAMP_FAILURE = "Failure to correctly clamp value to range.";
    private static final String MODULO_FAILURE =
            "Failure to correctly return a positive modulo value.";
    private static final String SMOOTH_STEP_FAILURE = "Failure to smooth step between 0 and 1.";

    @Test
    public void testRoundTwoDecimalPlaces() {
        Assert.assertEquals(ROUND_FAILURE, 2.12, MathUtils.roundTwoDecimalPlaces(2.123), EPSILON);
        Assert.assertEquals(ROUND_FAILURE, 2.13, MathUtils.roundTwoDecimalPlaces(2.127), EPSILON);
        Assert.assertEquals(ROUND_FAILURE, -2.12, MathUtils.roundTwoDecimalPlaces(-2.123), EPSILON);
        Assert.assertEquals(ROUND_FAILURE, -2.13, MathUtils.roundTwoDecimalPlaces(-2.127), EPSILON);
    }

    @Test
    public void testClampInt() {
        int min = 1;
        int max = 9;
        Assert.assertEquals(CLAMP_FAILURE, 4, MathUtils.clamp(4, min, max));
        Assert.assertEquals(CLAMP_FAILURE, 4, MathUtils.clamp(4, max, min));

        Assert.assertEquals(CLAMP_FAILURE, 1, MathUtils.clamp(-1, min, max));
        Assert.assertEquals(CLAMP_FAILURE, 1, MathUtils.clamp(0, max, min));

        Assert.assertEquals(CLAMP_FAILURE, 9, MathUtils.clamp(10, min, max));
        Assert.assertEquals(CLAMP_FAILURE, 9, MathUtils.clamp(30, max, min));
    }

    @Test
    public void testClampLong() {
        long min = 1L;
        long max = 9L;
        Assert.assertEquals(CLAMP_FAILURE, 4, (float) MathUtils.clamp(4, min, max), EPSILON);
        Assert.assertEquals(CLAMP_FAILURE, 4, (float) MathUtils.clamp(4, max, min), EPSILON);

        Assert.assertEquals(CLAMP_FAILURE, 1, (float) MathUtils.clamp(-1, min, max), EPSILON);
        Assert.assertEquals(CLAMP_FAILURE, 1, (float) MathUtils.clamp(0, max, min), EPSILON);

        Assert.assertEquals(CLAMP_FAILURE, 9, (float) MathUtils.clamp(10, min, max), EPSILON);
        Assert.assertEquals(CLAMP_FAILURE, 9, (float) MathUtils.clamp(30, max, min), EPSILON);
    }

    @Test
    public void testClampFloat() {
        float min = 1.0f;
        float max = 9.0f;
        Assert.assertEquals(CLAMP_FAILURE, 4.8f, MathUtils.clamp(4.8f, min, max), EPSILON);
        Assert.assertEquals(CLAMP_FAILURE, 4.8f, MathUtils.clamp(4.8f, max, min), EPSILON);

        Assert.assertEquals(CLAMP_FAILURE, 1f, MathUtils.clamp(-1.7f, min, max), EPSILON);
        Assert.assertEquals(CLAMP_FAILURE, 1f, MathUtils.clamp(0.003f, max, min), EPSILON);

        Assert.assertEquals(CLAMP_FAILURE, 9f, MathUtils.clamp(10.9f, min, max), EPSILON);
        Assert.assertEquals(CLAMP_FAILURE, 9f, MathUtils.clamp(30.1f, max, min), EPSILON);
    }

    @Test
    public void testPositiveModulo() {
        Assert.assertEquals(MODULO_FAILURE, 1, MathUtils.positiveModulo(3, 2));
        Assert.assertEquals(MODULO_FAILURE, 1, MathUtils.positiveModulo(3, -2));
        Assert.assertEquals(MODULO_FAILURE, 1, MathUtils.positiveModulo(-3, 2));
    }

    @Test
    public void testSmoothStep() {
        Assert.assertEquals(SMOOTH_STEP_FAILURE, 0f, MathUtils.smoothstep(0f), EPSILON);
        Assert.assertEquals(SMOOTH_STEP_FAILURE, 1f, MathUtils.smoothstep(1f), EPSILON);
        Assert.assertEquals(SMOOTH_STEP_FAILURE, 0.648f, MathUtils.smoothstep(0.6f), EPSILON);
        Assert.assertEquals(SMOOTH_STEP_FAILURE, 0.216f, MathUtils.smoothstep(0.3f), EPSILON);
    }
}
