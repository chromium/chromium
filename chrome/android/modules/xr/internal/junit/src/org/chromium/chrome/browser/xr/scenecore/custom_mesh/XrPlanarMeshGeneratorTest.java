// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore.custom_mesh;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;

/** Tests for {@link XrPlanarMeshGenerator.Config}. */
@RunWith(BaseRobolectricTestRunner.class)
public class XrPlanarMeshGeneratorTest {
    private static final float DELTA = 0.01f;
    private static final float WIDTH = 2.0f;
    private static final float HEIGHT = 1.0f;
    private static final float CORNER_RADIUS = 0.15f;
    private static final int CORNER_RESOLUTION = 12;
    private static final int TEXTURE_WIDTH = 1920;
    private static final int TEXTURE_HEIGHT = 1080;

    @Test
    public void testConfigDefaultsAndSetters() {
        XrPlanarMeshGenerator.Config config =
                new XrPlanarMeshGenerator.Config(
                        XrSurfaceEntityStereoMode.MONO,
                        TEXTURE_WIDTH,
                        TEXTURE_HEIGHT,
                        WIDTH,
                        HEIGHT,
                        CORNER_RADIUS,
                        CORNER_RESOLUTION);

        assertEquals(XrSurfaceEntityStereoMode.MONO, config.getStereoMode());
        assertEquals(TEXTURE_WIDTH, config.getTextureWidth());
        assertEquals(TEXTURE_HEIGHT, config.getTextureHeight());
        assertEquals(WIDTH, config.getWidth(), DELTA);
        assertEquals(HEIGHT, config.getHeight(), DELTA);
        assertEquals(CORNER_RADIUS, config.getCornerRadius(), DELTA);
        assertEquals(CORNER_RESOLUTION, config.getCornerResolution());
        assertEquals(XrPlanarMeshGenerator.Config.DEFAULT_FLIP_UV, config.getFlipUv());

        config.setWidth(4.0f);
        config.setHeight(2.0f);
        config.setCornerRadius(0.3f);
        config.setCornerResolution(20);

        assertEquals(4.0f, config.getWidth(), DELTA);
        assertEquals(2.0f, config.getHeight(), DELTA);
        assertEquals(0.3f, config.getCornerRadius(), DELTA);
        assertEquals(20, config.getCornerResolution());
    }

    @Test
    public void testConfigClamping() {
        XrPlanarMeshGenerator.Config config =
                new XrPlanarMeshGenerator.Config(
                        XrSurfaceEntityStereoMode.MONO,
                        /* textureWidth= */ -10,
                        /* textureHeight= */ 0,
                        /* width= */ -5.0f,
                        /* height= */ -2.0f,
                        /* cornerRadius= */ -1.0f,
                        /* cornerResolution= */ -2,
                        /* flipUv= */ false);

        assertEquals(1, config.getTextureWidth());
        assertEquals(1, config.getTextureHeight());
        assertEquals(0.0f, config.getWidth(), DELTA);
        assertEquals(0.0f, config.getHeight(), DELTA);
        assertEquals(0.0f, config.getCornerRadius(), DELTA);
        assertEquals(
                XrPlanarMeshGenerator.Config.MIN_CORNER_RESOLUTION, config.getCornerResolution());
    }
}
