// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore.custom_mesh;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.annotation.SuppressLint;

import androidx.xr.scenecore.SurfaceEntity.Shape;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;

import java.nio.FloatBuffer;

/** Tests for {@link XrSeamlessSphereMeshGenerator}. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressLint("RestrictedApiAndroidX")
public class XrSeamlessSphereMeshGeneratorTest {
    private static final float DELTA = 0.01f;
    private static final float EPSILON = 1e-4f;
    private static final float RADIUS = 2.0f;
    private static final int RESOLUTION = 50;
    private static final int TEXTURE_WIDTH = 100;
    private static final int TEXTURE_HEIGHT = 100;

    @Test
    public void testToCustomMesh_Mono() {
        XrSeamlessSphereMeshGenerator sphere =
                new XrSeamlessSphereMeshGenerator(
                        new XrCurvedMeshGenerator.Config(
                                XrSurfaceEntityStereoMode.MONO, TEXTURE_WIDTH, TEXTURE_HEIGHT));
        Shape.CustomMesh mesh = sphere.toCustomMesh();

        assertNotNull(mesh);
        assertNotNull(mesh.getLeftEye());
        assertNull(mesh.getRightEye());
    }

    @Test
    public void testToCustomMesh_SideBySide() {
        XrSeamlessSphereMeshGenerator sphere =
                new XrSeamlessSphereMeshGenerator(
                        new XrCurvedMeshGenerator.Config(
                                XrSurfaceEntityStereoMode.SIDE_BY_SIDE,
                                TEXTURE_WIDTH * 2,
                                TEXTURE_HEIGHT));
        Shape.CustomMesh mesh = sphere.toCustomMesh();

        assertNotNull(mesh);
        assertNotNull(mesh.getLeftEye());
        assertNotNull(mesh.getRightEye());
    }

    @Test
    public void testToCustomMesh_TopBottom() {
        XrSeamlessSphereMeshGenerator sphere =
                new XrSeamlessSphereMeshGenerator(
                        new XrCurvedMeshGenerator.Config(
                                XrSurfaceEntityStereoMode.TOP_BOTTOM,
                                TEXTURE_WIDTH,
                                TEXTURE_HEIGHT * 2));
        Shape.CustomMesh mesh = sphere.toCustomMesh();

        assertNotNull(mesh);
        assertNotNull(mesh.getLeftEye());
        assertNotNull(mesh.getRightEye());
    }

    @Test
    public void testSphereGeometry_CenterVideoFacesForward() {
        XrSeamlessSphereMeshGenerator sphere =
                new XrSeamlessSphereMeshGenerator(
                        new XrCurvedMeshGenerator.Config(
                                XrSurfaceEntityStereoMode.MONO,
                                TEXTURE_WIDTH,
                                TEXTURE_HEIGHT,
                                RADIUS,
                                RESOLUTION));
        Shape.CustomMesh customMesh = sphere.toCustomMesh();
        Shape.TriangleMesh mesh = customMesh.getLeftEye();
        assertNotNull(mesh);

        FloatBuffer posBuf = mesh.getPositions();

        // Equator ring index (lat = RESOLUTION / 2)
        int equatorLatIndex = RESOLUTION / 2;

        // Center of video frame, U = 0.5 (lon = RESOLUTION / 2)
        int centerLonIndex = RESOLUTION / 2;
        int centerVertIndex =
                RESOLUTION + (equatorLatIndex - 1) * (RESOLUTION + 1) + centerLonIndex;

        float x = posBuf.get(centerVertIndex * 3);
        float y = posBuf.get(centerVertIndex * 3 + 1);
        float z = posBuf.get(centerVertIndex * 3 + 2);

        // Center of video (U = 0.5) should be at x = 0, y = 0, z = -radius (facing -Z)
        assertEquals(0.0f, x, DELTA);
        assertEquals(0.0f, y, DELTA);
        assertEquals(-RADIUS, z, DELTA);
    }

    @Test
    public void testSeamPaddingInUv_Mono() {
        XrSeamlessSphereMeshGenerator sphere =
                new XrSeamlessSphereMeshGenerator(
                        new XrCurvedMeshGenerator.Config(
                                XrSurfaceEntityStereoMode.MONO,
                                TEXTURE_WIDTH,
                                TEXTURE_HEIGHT,
                                RADIUS,
                                RESOLUTION));
        Shape.CustomMesh customMesh = sphere.toCustomMesh();
        Shape.TriangleMesh mesh = customMesh.getLeftEye();
        assertNotNull(mesh);

        FloatBuffer uvBuf = mesh.getTexCoords();
        float expectedPaddingU =
                XrCurvedMeshGenerator.Config.DEFAULT_PADDING_TEXELS / TEXTURE_WIDTH;
        float expectedPaddingV =
                XrCurvedMeshGenerator.Config.DEFAULT_PADDING_TEXELS / TEXTURE_HEIGHT;

        // North pole duplicate vertex 0 (U = (0.5)/50 * range + pad)
        assertEquals(
                expectedPaddingU + (0.5f / RESOLUTION) * (1.0f - 2.0f * expectedPaddingU),
                uvBuf.get(0),
                EPSILON);
        // North pole V with flipUv = true: 1.0 - pad
        assertEquals(1.0f - expectedPaddingV, uvBuf.get(1), EPSILON);

        // First vertex of first latitude ring (lat = 1, lon = 0): U should be pad
        int firstRingFirstVertIndex = RESOLUTION;
        assertEquals(expectedPaddingU, uvBuf.get(firstRingFirstVertIndex * 2), EPSILON);

        // Last vertex of first latitude ring (lat = 1, lon = RESOLUTION): U should be 1 - pad
        int firstRingLastVertIndex = RESOLUTION + RESOLUTION;
        assertEquals(1.0f - expectedPaddingU, uvBuf.get(firstRingLastVertIndex * 2), EPSILON);

        // South pole duplicate vertex 0: V with flipUv = true: pad
        int spStartIndex = mesh.getPositions().capacity() / 3 - RESOLUTION;
        assertEquals(expectedPaddingV, uvBuf.get(spStartIndex * 2 + 1), EPSILON);
    }

    @Test
    public void testSeamPaddingInUv_SideBySide() {
        int textureWidth = 200;
        int textureHeight = 100;
        int eyeWidth = textureWidth / 2;
        int eyeHeight = textureHeight;

        XrSeamlessSphereMeshGenerator sphere =
                new XrSeamlessSphereMeshGenerator(
                        new XrCurvedMeshGenerator.Config(
                                XrSurfaceEntityStereoMode.SIDE_BY_SIDE,
                                textureWidth,
                                textureHeight,
                                RADIUS,
                                RESOLUTION));
        Shape.CustomMesh customMesh = sphere.toCustomMesh();

        float expectedPaddingU = XrCurvedMeshGenerator.Config.DEFAULT_PADDING_TEXELS / eyeWidth;

        // Left eye relative U range [0 + pad, 1.0 - pad]
        Shape.TriangleMesh leftMesh = customMesh.getLeftEye();
        assertNotNull(leftMesh);
        FloatBuffer leftUv = leftMesh.getTexCoords();
        int firstRingFirstVertIndex = RESOLUTION;
        int firstRingLastVertIndex = RESOLUTION + RESOLUTION;

        assertEquals(expectedPaddingU, leftUv.get(firstRingFirstVertIndex * 2), EPSILON);
        assertEquals(1.0f - expectedPaddingU, leftUv.get(firstRingLastVertIndex * 2), EPSILON);

        // Right eye relative U range [0 + pad, 1.0 - pad]
        Shape.TriangleMesh rightMesh = customMesh.getRightEye();
        assertNotNull(rightMesh);
        FloatBuffer rightUv = rightMesh.getTexCoords();

        assertEquals(expectedPaddingU, rightUv.get(firstRingFirstVertIndex * 2), EPSILON);
        assertEquals(1.0f - expectedPaddingU, rightUv.get(firstRingLastVertIndex * 2), EPSILON);
    }

    @Test
    public void testSeamPaddingInUv_TopBottom() {
        int textureWidth = 100;
        int textureHeight = 200;
        int eyeWidth = textureWidth;
        int eyeHeight = textureHeight / 2;

        XrSeamlessSphereMeshGenerator sphere =
                new XrSeamlessSphereMeshGenerator(
                        new XrCurvedMeshGenerator.Config(
                                XrSurfaceEntityStereoMode.TOP_BOTTOM,
                                textureWidth,
                                textureHeight,
                                RADIUS,
                                RESOLUTION));
        Shape.CustomMesh customMesh = sphere.toCustomMesh();

        float expectedPaddingV = XrCurvedMeshGenerator.Config.DEFAULT_PADDING_TEXELS / eyeHeight;

        // Left eye relative V range [0 + pad, 1.0 - pad] (with flipUv = true: top is 1 - pad,
        // bottom is pad)
        Shape.TriangleMesh leftMesh = customMesh.getLeftEye();
        assertNotNull(leftMesh);
        FloatBuffer leftUv = leftMesh.getTexCoords();

        assertEquals(1.0f - expectedPaddingV, leftUv.get(1), EPSILON);
        int spStartIndex = leftMesh.getPositions().capacity() / 3 - RESOLUTION;
        assertEquals(expectedPaddingV, leftUv.get(spStartIndex * 2 + 1), EPSILON);

        // Right eye relative V range [0 + pad, 1.0 - pad] (with flipUv = true: top is 1 - pad,
        // bottom is pad)
        Shape.TriangleMesh rightMesh = customMesh.getRightEye();
        assertNotNull(rightMesh);
        FloatBuffer rightUv = rightMesh.getTexCoords();

        assertEquals(1.0f - expectedPaddingV, rightUv.get(1), EPSILON);
        assertEquals(expectedPaddingV, rightUv.get(spStartIndex * 2 + 1), EPSILON);
    }

    @Test
    public void testSeamPaddingInUv_CustomPadding() {
        float customPaddingTexels = 0.4f;
        float expectedPaddingU = customPaddingTexels / TEXTURE_WIDTH;

        XrCurvedMeshGenerator.Config config =
                new XrCurvedMeshGenerator.Config(
                        XrSurfaceEntityStereoMode.MONO,
                        TEXTURE_WIDTH,
                        TEXTURE_HEIGHT,
                        RADIUS,
                        RESOLUTION);
        config.setPaddingTexels(customPaddingTexels);

        XrSeamlessSphereMeshGenerator sphere = new XrSeamlessSphereMeshGenerator(config);
        Shape.CustomMesh customMesh = sphere.toCustomMesh();
        Shape.TriangleMesh mesh = customMesh.getLeftEye();
        assertNotNull(mesh);

        FloatBuffer uvBuf = mesh.getTexCoords();
        int firstRingFirstVertIndex = RESOLUTION;
        assertEquals(expectedPaddingU, uvBuf.get(firstRingFirstVertIndex * 2), EPSILON);
    }
}
