// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore.custom_mesh;

import static org.junit.Assert.assertEquals;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;

/** Tests for {@link XrCurvedMeshGenerator.Config}. */
@RunWith(BaseRobolectricTestRunner.class)
public class XrCurvedMeshGeneratorTest {
    private static final float DELTA = 0.01f;
    private static final float RADIUS = 2.0f;
    private static final int RESOLUTION = 50;
    private static final int TEXTURE_WIDTH = 100;
    private static final int TEXTURE_HEIGHT = 100;

    @Test
    public void testConfigDefaultsAndSetters() {
        XrCurvedMeshGenerator.Config config =
                new XrCurvedMeshGenerator.Config(
                        XrSurfaceEntityStereoMode.MONO,
                        TEXTURE_WIDTH,
                        TEXTURE_HEIGHT,
                        RADIUS,
                        RESOLUTION);

        assertEquals(XrSurfaceEntityStereoMode.MONO, config.getStereoMode());
        assertEquals(TEXTURE_WIDTH, config.getTextureWidth());
        assertEquals(TEXTURE_HEIGHT, config.getTextureHeight());
        assertEquals(TEXTURE_WIDTH, config.getEyeWidth());
        assertEquals(TEXTURE_HEIGHT, config.getEyeHeight());
        assertEquals(RADIUS, config.getRadius(), DELTA);
        assertEquals(RESOLUTION, config.getResolution());
        assertEquals(XrCurvedMeshGenerator.Config.DEFAULT_FLIP_UV, config.getFlipUv());
        assertEquals(
                XrCurvedMeshGenerator.Config.DEFAULT_PADDING_TEXELS,
                config.getPaddingTexels(),
                DELTA);

        config.setStereoMode(XrSurfaceEntityStereoMode.SIDE_BY_SIDE);
        config.setSurfacePixelDimensions(TEXTURE_WIDTH * 2, TEXTURE_HEIGHT);
        config.setRadius(RADIUS * 2);
        config.setResolution(RESOLUTION * 2);
        config.setFlipUv(false);
        config.setPaddingTexels(0.5f);

        assertEquals(XrSurfaceEntityStereoMode.SIDE_BY_SIDE, config.getStereoMode());
        assertEquals(TEXTURE_WIDTH * 2, config.getTextureWidth());
        assertEquals(TEXTURE_HEIGHT, config.getTextureHeight());
        assertEquals(TEXTURE_WIDTH, config.getEyeWidth());
        assertEquals(TEXTURE_HEIGHT, config.getEyeHeight());
        assertEquals(RADIUS * 2, config.getRadius(), DELTA);
        assertEquals(RESOLUTION * 2, config.getResolution());
        assertEquals(false, config.getFlipUv());
        assertEquals(0.5f, config.getPaddingTexels(), DELTA);

        config.setStereoMode(XrSurfaceEntityStereoMode.TOP_BOTTOM);
        config.setSurfacePixelDimensions(TEXTURE_WIDTH, TEXTURE_HEIGHT * 2);

        assertEquals(XrSurfaceEntityStereoMode.TOP_BOTTOM, config.getStereoMode());
        assertEquals(TEXTURE_WIDTH, config.getTextureWidth());
        assertEquals(TEXTURE_HEIGHT * 2, config.getTextureHeight());
        assertEquals(TEXTURE_WIDTH, config.getEyeWidth());
        assertEquals(TEXTURE_HEIGHT, config.getEyeHeight());
    }

    @Test
    public void testConfigClamping() {
        XrCurvedMeshGenerator.Config config =
                new XrCurvedMeshGenerator.Config(
                        XrSurfaceEntityStereoMode.MONO,
                        /* textureWidth= */ -10,
                        /* textureHeight= */ 0,
                        /* radius= */ -5.0f,
                        /* resolution= */ 2,
                        /* flipUv= */ false,
                        /* paddingTexels= */ -1.0f);

        assertEquals(1, config.getTextureWidth());
        assertEquals(1, config.getTextureHeight());
        assertEquals(0.0f, config.getRadius(), DELTA);
        assertEquals(XrCurvedMeshGenerator.Config.MIN_RESOLUTION, config.getResolution());
        assertEquals(0.0f, config.getPaddingTexels(), DELTA);
    }
}
