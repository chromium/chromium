// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;

import android.annotation.SuppressLint;

import androidx.xr.scenecore.SurfaceEntity.DrawMode;
import androidx.xr.scenecore.SurfaceEntity.Shape;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.xr.scenecore.XrMeshData;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

/** Tests for {@link XrSurfaceEntityUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressLint("RestrictedApiAndroidX")
public class XrSurfaceEntityUtilsTest {

    private XrMeshData createValidMeshData(int textureId, int indexType) {
        float[] vertexData = {
            1.0f, 2.0f, 3.0f, 0.1f, 0.2f,
            4.0f, 5.0f, 6.0f, 0.4f, 0.5f
        };
        ByteBuffer vertices =
                ByteBuffer.allocateDirect(vertexData.length * 4).order(ByteOrder.nativeOrder());
        vertices.asFloatBuffer().put(vertexData);

        int[] indexData = {0, 1};
        ByteBuffer indices =
                ByteBuffer.allocateDirect(indexData.length * 4).order(ByteOrder.nativeOrder());
        indices.asIntBuffer().put(indexData);

        return new XrMeshData(textureId, indexType, vertices, indices);
    }

    private XrMeshData createInvalidMeshData() {
        // positions and texture coords will be null because interleavedVertices is null
        return new XrMeshData(1, 0, null, null);
    }

    @Test
    public void testCreateCustomMesh_EmptyArray() {
        assertNull(XrSurfaceEntityUtils.createCustomMesh(new XrMeshData[0]));
    }

    @Test
    public void testCreateCustomMesh_NullFirstElement() {
        assertNull(XrSurfaceEntityUtils.createCustomMesh(new XrMeshData[] {null}));
    }

    @Test
    public void testCreateCustomMesh_InvalidFirstElement() {
        XrMeshData invalidData = createInvalidMeshData();
        assertNull(XrSurfaceEntityUtils.createCustomMesh(new XrMeshData[] {invalidData}));
    }

    @Test
    public void testCreateCustomMesh_ValidMono() {
        XrMeshData data = createValidMeshData(10, 0); // indexType 0 -> DrawMode.TRIANGLES
        Shape.CustomMesh customMesh =
                XrSurfaceEntityUtils.createCustomMesh(new XrMeshData[] {data});
        assertNotNull(customMesh);
        assertEquals(DrawMode.TRIANGLES, customMesh.getDrawMode());
        assertNotNull(customMesh.getLeftEye());
        assertNull(customMesh.getRightEye());

        Shape.TriangleMesh leftMesh = customMesh.getLeftEye();
        assertEquals(data.getPositionsAsFloatBuffer(), leftMesh.getPositions());
        assertEquals(data.getTextureCoordsAsFloatBuffer(), leftMesh.getTexCoords());
        assertEquals(data.getIndicesAsIntBuffer(), leftMesh.getIndices());
    }

    @Test
    public void testCreateCustomMesh_ValidStereo() {
        XrMeshData dataLeft = createValidMeshData(10, 1); // indexType 1 -> DrawMode.TRIANGLE_STRIP
        XrMeshData dataRight = createValidMeshData(11, 1);
        Shape.CustomMesh customMesh =
                XrSurfaceEntityUtils.createCustomMesh(new XrMeshData[] {dataLeft, dataRight});
        assertNotNull(customMesh);
        assertEquals(DrawMode.TRIANGLE_STRIP, customMesh.getDrawMode());
        assertNotNull(customMesh.getLeftEye());
        assertNotNull(customMesh.getRightEye());
    }

    @Test
    public void testCreateCustomMesh_DrawModes() {
        // DrawMode.TRIANGLE_FAN for indexType 2
        XrMeshData dataFan = createValidMeshData(10, 2);
        Shape.CustomMesh meshFan =
                XrSurfaceEntityUtils.createCustomMesh(new XrMeshData[] {dataFan});
        assertNotNull(meshFan);
        assertEquals(DrawMode.TRIANGLE_FAN, meshFan.getDrawMode());

        // Unknown indexType (e.g. 3) should throw IllegalArgumentException
        XrMeshData dataUnknown = createValidMeshData(10, 3);
        assertThrows(
                IllegalArgumentException.class,
                () -> XrSurfaceEntityUtils.createCustomMesh(new XrMeshData[] {dataUnknown}));
    }
}
