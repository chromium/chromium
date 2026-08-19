// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.xr.scenecore.XrPixelDensity;

/** Tests for {@link XrPixelDensityImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class XrPixelDensityImplTest {
    private static final float DELTA = 0.01f;
    private static final float DP_PER_METER = 1000f;
    private static final float DISPLAY_DENSITY = 2.0f;

    private XrPixelDensity mPixelDensity;

    @Before
    public void setUp() {
        mPixelDensity =
                XrPixelDensityImpl.createForTesting(DP_PER_METER, DP_PER_METER * DISPLAY_DENSITY);
    }

    @Test
    public void testGetDpPerMeter() {
        assertNotNull(mPixelDensity);
        assertEquals(DP_PER_METER, mPixelDensity.getDpPerMeter(), DELTA);
    }

    @Test
    public void testGetPixelsPerMeter() {
        assertNotNull(mPixelDensity);
        assertEquals(DP_PER_METER * DISPLAY_DENSITY, mPixelDensity.getPixelsPerMeter(), DELTA);
    }

    @Test
    public void testConvertDpToMeters() {
        assertEquals(1.0f, mPixelDensity.convertDpToMeters(1000f), DELTA);
        assertEquals(0.5f, mPixelDensity.convertDpToMeters(500f), DELTA);
        assertEquals(0.0f, mPixelDensity.convertDpToMeters(0f), DELTA);
    }

    @Test
    public void testConvertMetersToDp() {
        assertEquals(1000f, mPixelDensity.convertMetersToDp(1.0f), DELTA);
        assertEquals(500f, mPixelDensity.convertMetersToDp(0.5f), DELTA);
        assertEquals(0.0f, mPixelDensity.convertMetersToDp(0.0f), DELTA);
    }

    @Test
    public void testConvertPixelsToMeters() {
        assertEquals(1.0f, mPixelDensity.convertPixelsToMeters(2000f), DELTA);
        assertEquals(0.5f, mPixelDensity.convertPixelsToMeters(1000f), DELTA);
        assertEquals(0.0f, mPixelDensity.convertPixelsToMeters(0f), DELTA);
    }

    @Test
    public void testConvertMetersToPixels() {
        assertEquals(2000f, mPixelDensity.convertMetersToPixels(1.0f), DELTA);
        assertEquals(1000f, mPixelDensity.convertMetersToPixels(0.5f), DELTA);
        assertEquals(0.0f, mPixelDensity.convertMetersToPixels(0.0f), DELTA);
    }

    @Test
    public void testConvertZeroDpPerMeter() {
        XrPixelDensity zeroDensity = XrPixelDensityImpl.createForTesting(0f, 0f);
        assertEquals(0f, zeroDensity.convertDpToMeters(1000f), DELTA);
        assertEquals(0f, zeroDensity.convertMetersToDp(1.0f), DELTA);
        assertEquals(0f, zeroDensity.convertPixelsToMeters(1000f), DELTA);
        assertEquals(0f, zeroDensity.convertMetersToPixels(1.0f), DELTA);
    }

    @Test
    public void testCreateForTesting() {
        XrPixelDensity density = XrPixelDensityImpl.createForTesting(1000f, 2000f);
        assertNotNull(density);
        assertEquals(1000f, density.getDpPerMeter(), DELTA);
        assertEquals(2000f, density.getPixelsPerMeter(), DELTA);
    }


    @Test
    public void testCalculatePixelsPerMeter() {
        assertEquals(
                XrPixelDensityImpl.DEFAULT_PIXELS_PER_METER,
                XrPixelDensityImpl.calculatePixelsPerMeter(),
                DELTA);
    }

    @Test
    public void testStaticCreate() {
        XrPixelDensity density = XrPixelDensityImpl.create(DISPLAY_DENSITY);
        assertNotNull(density);
        assertEquals(
                XrPixelDensityImpl.DEFAULT_PIXELS_PER_METER / DISPLAY_DENSITY,
                density.getDpPerMeter(),
                DELTA);
        assertEquals(
                XrPixelDensityImpl.DEFAULT_PIXELS_PER_METER,
                density.getPixelsPerMeter(),
                DELTA);
    }
}
