// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import android.annotation.SuppressLint;

import androidx.xr.scenecore.SurfaceEntity.DrawMode;
import androidx.xr.scenecore.SurfaceEntity.Shape;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.xr.scenecore.XrMeshData;

import java.nio.FloatBuffer;

/** Utility class for {@link androidx.xr.scenecore.SurfaceEntity}. */
@NullMarked
@SuppressLint("RestrictedApiAndroidX")
public class XrSurfaceEntityUtils {
    private XrSurfaceEntityUtils() {}

    /** Creates a {@link Shape.CustomMesh} from the given {@link XrMeshData} array. */
    public static Shape.@Nullable CustomMesh createCustomMesh(XrMeshData[] meshDatas) {
        if (meshDatas.length == 0 || meshDatas[0] == null) return null;

        XrMeshData leftEyeMeshData = meshDatas[0];
        XrMeshData rightEyeMeshData = meshDatas.length > 1 ? meshDatas[1] : null;

        Shape.TriangleMesh leftEyeMesh = createTriangleMesh(leftEyeMeshData);
        if (leftEyeMesh == null) return null;

        Shape.TriangleMesh rightEyeMesh = createTriangleMesh(rightEyeMeshData);
        DrawMode drawMode = getDrawMode(leftEyeMeshData.getIndexType());

        return new Shape.CustomMesh(leftEyeMesh, rightEyeMesh, drawMode);
    }

    private static DrawMode getDrawMode(int indexType) {
        switch (indexType) {
            case 0:
                return DrawMode.TRIANGLES;
            case 1:
                return DrawMode.TRIANGLE_STRIP;
            case 2:
                return DrawMode.TRIANGLE_FAN;
            default:
                throw new IllegalArgumentException("Unknown index type: " + indexType);
        }
    }

    private static Shape.@Nullable TriangleMesh createTriangleMesh(@Nullable XrMeshData data) {
        if (data == null) return null;
        FloatBuffer positions = data.getPositionsAsFloatBuffer();
        FloatBuffer textureCoords = data.getTextureCoordsAsFloatBuffer();
        if (positions == null || textureCoords == null) {
            return null;
        }
        return new Shape.TriangleMesh(positions, textureCoords, data.getIndicesAsIntBuffer());
    }
}
