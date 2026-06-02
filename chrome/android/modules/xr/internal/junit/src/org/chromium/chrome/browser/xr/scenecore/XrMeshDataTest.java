// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThrows;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.xr.scenecore.XrMeshData;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;

/** Tests for {@link XrMeshData}. */
@RunWith(BaseRobolectricTestRunner.class)
public class XrMeshDataTest {
    private static final float DELTA = 0.001f;

    @Test
    public void testCreateArray() {
        XrMeshData[] array = XrMeshData.createArray(5);
        assertNotNull(array);
        assertEquals(5, array.length);
    }

    @Test
    public void testConstructor_NullOrEmptyVertices() {
        // Null interleavedVertices
        XrMeshData meshDataNull = new XrMeshData(42, 1, null, null);
        assertEquals(42, meshDataNull.getTextureId());
        assertEquals(1, meshDataNull.getIndexType());
        assertNull(meshDataNull.getPositions());
        assertNull(meshDataNull.getPositionsAsFloatBuffer());
        assertNull(meshDataNull.getTextureCoords());
        assertNull(meshDataNull.getTextureCoordsAsFloatBuffer());
        assertNull(meshDataNull.getIndices());
        assertNull(meshDataNull.getIndicesAsIntBuffer());

        // Empty interleavedVertices
        ByteBuffer emptyBuffer = ByteBuffer.allocateDirect(0);
        XrMeshData meshDataEmpty = new XrMeshData(42, 1, emptyBuffer, null);
        assertNull(meshDataEmpty.getPositions());
    }

    @Test
    public void testConstructor_InvalidVerticesLength() {
        // Length must be a multiple of 5 (e.g. 4 is invalid)
        ByteBuffer vertices = ByteBuffer.allocateDirect(4 * 4).order(ByteOrder.nativeOrder());
        assertThrows(IllegalArgumentException.class, () -> new XrMeshData(1, 0, vertices, null));
    }

    @Test
    public void testConstructor_ValidMeshData() {
        // 2 vertices, each having 5 floats: pos(x, y, z), tex(u, v)
        float[] vertexData = {
            1.0f, 2.0f, 3.0f, 0.1f, 0.2f,
            4.0f, 5.0f, 6.0f, 0.4f, 0.5f
        };
        ByteBuffer vertices =
                ByteBuffer.allocateDirect(vertexData.length * 4).order(ByteOrder.nativeOrder());
        vertices.asFloatBuffer().put(vertexData);

        // 3 indices (e.g. one triangle)
        int[] indexData = {0, 1, 0};
        ByteBuffer indices =
                ByteBuffer.allocateDirect(indexData.length * 4).order(ByteOrder.nativeOrder());
        indices.asIntBuffer().put(indexData);

        XrMeshData meshData = new XrMeshData(10, 2, vertices, indices);
        assertEquals(10, meshData.getTextureId());
        assertEquals(2, meshData.getIndexType());

        // Verify positions
        FloatBuffer posBuf = meshData.getPositionsAsFloatBuffer();
        assertNotNull(posBuf);
        assertEquals(6, posBuf.remaining());
        assertEquals(1.0f, posBuf.get(), DELTA);
        assertEquals(2.0f, posBuf.get(), DELTA);
        assertEquals(3.0f, posBuf.get(), DELTA);
        assertEquals(4.0f, posBuf.get(), DELTA);
        assertEquals(5.0f, posBuf.get(), DELTA);
        assertEquals(6.0f, posBuf.get(), DELTA);

        // Verify texture coords
        FloatBuffer texBuf = meshData.getTextureCoordsAsFloatBuffer();
        assertNotNull(texBuf);
        assertEquals(4, texBuf.remaining());
        assertEquals(0.1f, texBuf.get(), DELTA);
        assertEquals(0.2f, texBuf.get(), DELTA);
        assertEquals(0.4f, texBuf.get(), DELTA);
        assertEquals(0.5f, texBuf.get(), DELTA);

        // Verify indices
        IntBuffer indBuf = meshData.getIndicesAsIntBuffer();
        assertNotNull(indBuf);
        assertEquals(3, indBuf.remaining());
        assertEquals(0, indBuf.get());
        assertEquals(1, indBuf.get());
        assertEquals(0, indBuf.get());
    }

    @Test
    public void testApplyAndResetScale() {
        float[] vertexData = {
            1.0f, 2.0f, 3.0f, 0.1f, 0.2f,
            -1.0f, -2.0f, -3.0f, 0.4f, 0.5f
        };
        ByteBuffer vertices =
                ByteBuffer.allocateDirect(vertexData.length * 4).order(ByteOrder.nativeOrder());
        vertices.asFloatBuffer().put(vertexData);

        XrMeshData meshData = new XrMeshData(10, 2, vertices, null);

        // Apply scale of 2.0
        meshData.applyScale(2.0f);

        FloatBuffer posBuf = meshData.getPositionsAsFloatBuffer();
        assertNotNull(posBuf);
        assertEquals(2.0f, posBuf.get(), DELTA);
        assertEquals(4.0f, posBuf.get(), DELTA);
        assertEquals(6.0f, posBuf.get(), DELTA);
        assertEquals(-2.0f, posBuf.get(), DELTA);
        assertEquals(-4.0f, posBuf.get(), DELTA);
        assertEquals(-6.0f, posBuf.get(), DELTA);

        // Applying same scale shouldn't do anything
        posBuf.rewind();
        meshData.applyScale(2.0f);
        assertEquals(2.0f, posBuf.get(), DELTA);

        // Apply scale of 0.5 (relative to original scale of 1.0, so it is a scaling factor of 0.5 /
        // 2.0 = 0.25 on the current values)
        posBuf.rewind();
        meshData.applyScale(0.5f);
        assertEquals(0.5f, posBuf.get(), DELTA);
        assertEquals(1.0f, posBuf.get(), DELTA);
        assertEquals(1.5f, posBuf.get(), DELTA);

        // Reset scale
        posBuf.rewind();
        meshData.resetScale();
        assertEquals(1.0f, posBuf.get(), DELTA);
        assertEquals(2.0f, posBuf.get(), DELTA);
        assertEquals(3.0f, posBuf.get(), DELTA);
    }

    @Test
    public void testApplyScale_NullPositions() {
        XrMeshData meshData = new XrMeshData(10, 2, null, null);
        // Should not throw Exception
        meshData.applyScale(2.0f);
        assertNull(meshData.getPositions());
    }

    @Test
    public void testApplyScale_InvalidScale() {
        float[] vertexData = {1.0f, 2.0f, 3.0f, 0.1f, 0.2f};
        ByteBuffer vertices =
                ByteBuffer.allocateDirect(vertexData.length * 4).order(ByteOrder.nativeOrder());
        vertices.asFloatBuffer().put(vertexData);
        XrMeshData meshData = new XrMeshData(10, 2, vertices, null);

        assertThrows(IllegalArgumentException.class, () -> meshData.applyScale(0.0f));
        assertThrows(IllegalArgumentException.class, () -> meshData.applyScale(-1.5f));
    }
}
