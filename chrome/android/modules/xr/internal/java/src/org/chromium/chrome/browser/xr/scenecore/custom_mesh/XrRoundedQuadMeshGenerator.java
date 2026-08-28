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
 * Helper class that generates a 2D quad mesh with rounded corners for SceneCore.
 *
 * <p>Constructs a planar mesh geometry with smoothly rounded corners using a center-fan
 * triangulation, ensuring compatibility across all API levels.
 *
 * <p>TODO(crbug.com/550356627): Remove this class once updated to the latest SceneCore version that
 * supports rounded quad meshes natively.
 */
@NullMarked
public class XrRoundedQuadMeshGenerator extends XrPlanarMeshGenerator {

    public XrRoundedQuadMeshGenerator(Config config) {
        super(config);
    }

    @Override
    protected XrMeshData generateMeshData() {
        float width = mConfig.getWidth();
        float height = mConfig.getHeight();
        float cornerRadius = mConfig.getCornerRadius();
        boolean flipUv = mConfig.getFlipUv();
        int cornerResolution = mConfig.getCornerResolution();

        float halfW = width / 2.0f;
        float halfH = height / 2.0f;
        float maxR = Math.min(halfW, halfH);
        float r = Math.max(0.0f, Math.min(cornerRadius, maxR));

        int perimeterPoints = 4 * cornerResolution;
        int totalVertices = 1 + perimeterPoints;
        int totalTriangles = perimeterPoints;
        int totalIndices = totalTriangles * 3;

        ByteBuffer vertexBuffer =
                ByteBuffer.allocateDirect(totalVertices * 5 * Float.BYTES)
                        .order(ByteOrder.nativeOrder());
        FloatBuffer vertices = vertexBuffer.asFloatBuffer();

        ByteBuffer indexBuffer =
                ByteBuffer.allocateDirect(totalIndices * Integer.BYTES)
                        .order(ByteOrder.nativeOrder());
        IntBuffer indices = indexBuffer.asIntBuffer();

        // 1. Center vertex (index 0)
        vertices.put(0.0f);
        vertices.put(0.0f);
        vertices.put(0.0f);
        vertices.put(0.5f);
        vertices.put(0.5f);

        // 2. Corner centers:
        // Corner 0 (Top-Right):    (+halfW - r, +halfH - r)
        // Corner 1 (Top-Left):     (-halfW + r, +halfH - r)
        // Corner 2 (Bottom-Left):  (-halfW + r, -halfH + r)
        // Corner 3 (Bottom-Right): (+halfW - r, -halfH + r)
        float[] cx = new float[] {halfW - r, -halfW + r, -halfW + r, halfW - r};
        float[] cy = new float[] {halfH - r, halfH - r, -halfH + r, -halfH + r};

        // 3. Perimeter vertices:
        for (int c = 0; c < 4; ++c) {
            double startAngle = c * (Math.PI / 2.0);
            for (int j = 0; j < cornerResolution; ++j) {
                double angle = startAngle + (j / (double) cornerResolution) * (Math.PI / 2.0);
                float x = cx[c] + r * (float) Math.cos(angle);
                float y = cy[c] + r * (float) Math.sin(angle);

                float u = (x + halfW) / width;
                float normY = (y + halfH) / height;
                float v = flipUv ? normY : (1.0f - normY);

                vertices.put(x);
                vertices.put(y);
                vertices.put(0.0f);
                vertices.put(u);
                vertices.put(v);
            }
        }

        // 4. Triangle fan indices connecting center (0) to perimeter
        for (int i = 0; i < perimeterPoints; ++i) {
            int current = 1 + i;
            int next = 1 + ((i + 1) % perimeterPoints);

            indices.put(0);
            indices.put(current);
            indices.put(next);
        }

        vertexBuffer.rewind();
        indexBuffer.rewind();

        return new XrMeshData(/* textureId= */ 0, /* indexType= */ 0, vertexBuffer, indexBuffer);
    }
}
