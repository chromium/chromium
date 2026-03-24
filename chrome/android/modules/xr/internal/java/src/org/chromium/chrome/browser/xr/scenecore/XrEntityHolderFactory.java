// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import android.view.View;

import androidx.xr.runtime.Session;
import androidx.xr.runtime.math.FloatSize2d;
import androidx.xr.runtime.math.Pose;
import androidx.xr.scenecore.PanelEntity;
import androidx.xr.scenecore.SurfaceEntity;
import androidx.xr.scenecore.SurfaceEntity.Shape;
import androidx.xr.scenecore.SurfaceEntity.StereoMode;
import androidx.xr.scenecore.SurfaceEntity.SuperSampling;
import androidx.xr.scenecore.SurfaceEntity.SurfaceProtection;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrCurvedSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
import org.chromium.ui.xr.scenecore.XrQuadSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;

/**
 * Factory for creating XR entity holders.
 *
 * <p>TODO(crbug.com/495766632): Add test coverage for this implementation.
 */
@NullMarked
public class XrEntityHolderFactory {

    public static XrSurfaceEntityHolder createSurfaceEntityHolder(
            Session xrSession, @XrSurfaceEntityShape int shape) {
        switch (shape) {
            case XrSurfaceEntityShape.QUAD:
                return createQuadSurfaceEntityHolder(xrSession);
            case XrSurfaceEntityShape.SPHERE:
                return createSphereSurfaceEntityHolder(xrSession);
            case XrSurfaceEntityShape.HEMISPHERE:
                return createHemisphereSurfaceEntityHolder(xrSession);
            default:
                throw new IllegalArgumentException("Invalid shape: " + shape);
        }
    }

    public static XrQuadSurfaceEntityHolder createQuadSurfaceEntityHolder(Session xrSession) {
        return XrSurfaceEntityHolderImpl.create(
                xrSession,
                SurfaceEntity.create(
                        xrSession,
                        Pose.Identity,
                        new Shape.Quad(new FloatSize2d(1f, 1f)),
                        StereoMode.MONO,
                        SuperSampling.PENTAGON,
                        SurfaceProtection.NONE));
    }

    public static XrCurvedSurfaceEntityHolder createSphereSurfaceEntityHolder(Session xrSession) {
        return XrSurfaceEntityHolderImpl.create(
                xrSession,
                SurfaceEntity.create(
                        xrSession,
                        Pose.Identity,
                        new Shape.Sphere(1f),
                        StereoMode.MONO,
                        SuperSampling.PENTAGON,
                        SurfaceProtection.NONE));
    }

    public static XrCurvedSurfaceEntityHolder createHemisphereSurfaceEntityHolder(
            Session xrSession) {
        return XrSurfaceEntityHolderImpl.create(
                xrSession,
                SurfaceEntity.create(
                        xrSession,
                        Pose.Identity,
                        new Shape.Hemisphere(1f),
                        StereoMode.MONO,
                        SuperSampling.PENTAGON,
                        SurfaceProtection.NONE));
    }

    public static XrPanelEntityHolder createPanelEntityHolder(
            Session xrSession, View view, String name) {
        return XrPanelEntityHolderImpl.create(
                xrSession,
                PanelEntity.create(xrSession, view, new FloatSize2d(1f, 1f), name, Pose.Identity));
    }
}
