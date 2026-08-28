// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore.custom_mesh;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import androidx.xr.scenecore.SurfaceEntity.Shape;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;

import java.nio.FloatBuffer;
import java.nio.IntBuffer;

/**
 * Tests for {@link XrRoundedQuadMeshGenerator}.
 *
 * <p>TODO(crbug.com/550356627): Remove this class once updated to the latest SceneCore version that
 * supports rounded quad meshes natively.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class XrRoundedQuadMeshGeneratorTest {
    private static final float DELTA = 1e-4f;

    @Test
    public void testToCustomMesh_Mono() {
        var config =
                new XrPlanarMeshGenerator.Config(
                        XrSurfaceEntityStereoMode.MONO,
                        /* textureWidth= */ 1920,
                        /* textureHeight= */ 1080);
        config.setWidth(2.0f);
        config.setHeight(1.0f);
        config.setCornerRadius(0.1f);
        config.setCornerResolution(10);

        var generator = new XrRoundedQuadMeshGenerator(config);
        Shape.CustomMesh customMesh = generator.toCustomMesh();

        assertNotNull(customMesh);
        assertNotNull(customMesh.getLeftEye());
        assertNull(customMesh.getRightEye());

        Shape.TriangleMesh leftEye = customMesh.getLeftEye();
        FloatBuffer positions = leftEye.getPositions();
        FloatBuffer texCoords = leftEye.getTexCoords();
        IntBuffer indices = leftEye.getIndices();

        // 1 center + 4 * 10 corner vertices = 41 vertices
        int expectedVertices = 1 + 4 * 10;
        assertEquals(expectedVertices * 3, positions.capacity());
        assertEquals(expectedVertices * 2, texCoords.capacity());
        // 4 * 10 triangles = 40 triangles = 120 indices
        assertEquals(4 * 10 * 3, indices.capacity());

        // Verify center vertex
        assertEquals(0.0f, positions.get(0), DELTA);
        assertEquals(0.0f, positions.get(1), DELTA);
        assertEquals(0.0f, positions.get(2), DELTA);
        assertEquals(0.5f, texCoords.get(0), DELTA);
        assertEquals(0.5f, texCoords.get(1), DELTA);
    }

    @Test
    public void testToCustomMesh_StereoReusesMesh() {
        var config =
                new XrPlanarMeshGenerator.Config(
                        XrSurfaceEntityStereoMode.SIDE_BY_SIDE,
                        /* textureWidth= */ 3840,
                        /* textureHeight= */ 1080);
        config.setWidth(2.0f);
        config.setHeight(1.0f);
        config.setCornerRadius(0.1f);
        config.setCornerResolution(10);

        var generator = new XrRoundedQuadMeshGenerator(config);
        Shape.CustomMesh customMesh = generator.toCustomMesh();

        assertNotNull(customMesh);
        assertNotNull(customMesh.getLeftEye());
        assertNotNull(customMesh.getRightEye());

        FloatBuffer leftTexCoords = customMesh.getLeftEye().getTexCoords();
        FloatBuffer rightTexCoords = customMesh.getRightEye().getTexCoords();

        // Left and right eye share identical mesh geometry and UVs
        assertEquals(0.5f, leftTexCoords.get(0), DELTA);
        assertEquals(0.5f, rightTexCoords.get(0), DELTA);
    }

    @Test
    public void testRadiusClampedToHalfDimension() {
        var config =
                new XrPlanarMeshGenerator.Config(
                        XrSurfaceEntityStereoMode.MONO,
                        /* textureWidth= */ 1920,
                        /* textureHeight= */ 1080);
        config.setWidth(1.0f);
        config.setHeight(0.5f);
        // Radius greater than half height (0.25)
        config.setCornerRadius(0.8f);

        var generator = new XrRoundedQuadMeshGenerator(config);
        Shape.CustomMesh customMesh = generator.toCustomMesh();
        assertNotNull(customMesh);
    }
}
