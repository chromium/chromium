// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore.custom_mesh;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrMeshData;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;
import java.nio.IntBuffer;

/**
 * Helper class that generates a seamless UV-mapped sphere mesh for SceneCore.
 *
 * <p>Generates custom mesh geometry for rendering 360-degree immersive content in SceneCore.
 *
 * <p>Includes inward texture coordinate padding to prevent bilinear filtering seam artifacts along
 * texture edges.
 */
@NullMarked
public class XrSeamlessSphereMeshGenerator extends XrCurvedMeshGenerator {

    public XrSeamlessSphereMeshGenerator(Config config) {
        super(config);
    }

    @Override
    protected XrMeshData generateMeshData() {
        int resolution = mConfig.getResolution();
        float radius = mConfig.getRadius();
        boolean flipUv = mConfig.getFlipUv();
        int eyeWidth = mConfig.getEyeWidth();
        int eyeHeight = mConfig.getEyeHeight();
        int verticesPerRing = resolution + 1;
        int numberOfVertices = verticesPerRing * (resolution - 1) + 2 * resolution;
        int numberOfIndices = resolution * (resolution - 1) * 6;

        ByteBuffer vertexBuffer =
                ByteBuffer.allocateDirect(numberOfVertices * 5 * Float.BYTES)
                        .order(ByteOrder.nativeOrder());
        FloatBuffer vertices = vertexBuffer.asFloatBuffer();

        ByteBuffer indexBuffer =
                ByteBuffer.allocateDirect(numberOfIndices * Integer.BYTES)
                        .order(ByteOrder.nativeOrder());
        IntBuffer indices = indexBuffer.asIntBuffer();

        float paddingTexels = mConfig.getPaddingTexels();
        float uPadding = paddingTexels / (float) eyeWidth;
        float vPadding = paddingTexels / (float) eyeHeight;
        float uMin = uPadding;
        float uRange = 1.0f - 2.0f * uPadding;
        float vMin = vPadding;
        float vRange = 1.0f - 2.0f * vPadding;

        // North pole duplicate vertices
        float northV = flipUv ? (vMin + vRange) : vMin;
        for (int j = 0; j < resolution; ++j) {
            float u = uMin + ((j + 0.5f) / resolution) * uRange;
            vertices.put(0.0f);
            vertices.put(radius);
            vertices.put(0.0f);
            vertices.put(u);
            vertices.put(northV);
        }

        // Intermediate latitude rings
        for (int i = 1; i < resolution; ++i) {
            float phi = (float) i / resolution * (float) Math.PI;
            float y = radius * (float) Math.cos(phi);
            float ringRadius = radius * (float) Math.sin(phi);
            float rawV = (float) i / resolution;
            float v = flipUv ? (vMin + (1.0f - rawV) * vRange) : (vMin + rawV * vRange);

            for (int j = 0; j <= resolution; ++j) {
                float theta = (float) j / resolution * 2.0f * (float) Math.PI;
                float x = ringRadius * (float) Math.sin(theta);
                float z = ringRadius * (float) Math.cos(theta);
                float u = uMin + ((float) j / resolution) * uRange;

                vertices.put(x);
                vertices.put(y);
                vertices.put(z);
                vertices.put(u);
                vertices.put(v);
            }
        }

        // South pole duplicate vertices
        float southV = flipUv ? vMin : (vMin + vRange);
        for (int j = 0; j < resolution; ++j) {
            float u = uMin + ((j + 0.5f) / resolution) * uRange;
            vertices.put(0.0f);
            vertices.put(-radius);
            vertices.put(0.0f);
            vertices.put(u);
            vertices.put(southV);
        }

        // North pole cap triangles (inward-facing winding)
        int firstRingOffset = resolution;
        for (int j = 0; j < resolution; ++j) {
            int poleIndex = j;
            int ringLeft = firstRingOffset + j;
            int ringRight = firstRingOffset + j + 1;

            indices.put(poleIndex);
            indices.put(ringRight);
            indices.put(ringLeft);
        }

        // Body quads (split into 2 triangles each)
        for (int i = 0; i < resolution - 2; ++i) {
            int currentRingOffset = resolution + i * verticesPerRing;
            int nextRingOffset = currentRingOffset + verticesPerRing;

            for (int j = 0; j < resolution; ++j) {
                int topLeft = currentRingOffset + j;
                int topRight = currentRingOffset + j + 1;
                int bottomLeft = nextRingOffset + j;
                int bottomRight = nextRingOffset + j + 1;

                indices.put(topLeft);
                indices.put(topRight);
                indices.put(bottomLeft);

                indices.put(topRight);
                indices.put(bottomRight);
                indices.put(bottomLeft);
            }
        }

        // South pole cap triangles (inward-facing winding)
        int lastRingOffset = resolution + (resolution - 2) * verticesPerRing;
        int southPoleOffset = numberOfVertices - resolution;
        for (int j = 0; j < resolution; ++j) {
            int ringLeft = lastRingOffset + j;
            int ringRight = lastRingOffset + j + 1;
            int poleIndex = southPoleOffset + j;

            indices.put(poleIndex);
            indices.put(ringLeft);
            indices.put(ringRight);
        }

        vertexBuffer.rewind();
        indexBuffer.rewind();

        return new XrMeshData(/* textureId= */ 0, /* indexType= */ 0, vertexBuffer, indexBuffer);
    }
}
