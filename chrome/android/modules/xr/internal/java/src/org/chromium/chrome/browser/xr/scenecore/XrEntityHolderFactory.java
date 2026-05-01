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

/** Factory for creating XR entity holders. */
@NullMarked
public class XrEntityHolderFactory {

    /**
     * Creates an XR surface entity holder with the given shape.
     *
     * @param xrSession The XR session to use.
     * @param shape The shape of the surface entity.
     * @return An XR surface entity holder.
     */
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

    /**
     * Creates an XR quad surface entity holder.
     *
     * @param xrSession The XR session to use.
     * @return An XR quad surface entity holder.
     */
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

    /**
     * Creates an XR sphere surface entity holder.
     *
     * @param xrSession The XR session to use.
     * @return An XR sphere surface entity holder.
     */
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

    /**
     * Creates an XR hemisphere surface entity holder.
     *
     * @param xrSession The XR session to use.
     * @return An XR hemisphere surface entity holder.
     */
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

    /**
     * Creates an XR panel entity holder.
     *
     * @param xrSession The XR session to use.
     * @param view The view to use.
     * @param name The name of the panel.
     * @return An XR panel entity holder.
     */
    public static XrPanelEntityHolder createPanelEntityHolder(
            Session xrSession, View view, String name) {
        return XrPanelEntityHolderImpl.create(
                xrSession,
                PanelEntity.create(xrSession, view, new FloatSize2d(1f, 1f), name, Pose.Identity));
    }
}
