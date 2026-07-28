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
import org.chromium.ui.xr.scenecore.XrFactory;
import org.chromium.ui.xr.scenecore.XrFloatSize3d;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrQuaternion;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrVector3;

/** Concrete implementation of {@link XrFactory}. */
@NullMarked
public class XrFactoryImpl implements XrFactory {
    @Override
    public XrVector3 createVector3(float x, float y, float z) {
        return XrVector3Impl.create(x, y, z);
    }

    @Override
    public XrQuaternion createQuaternion(float x, float y, float z, float w) {
        return XrQuaternionImpl.create(x, y, z, w);
    }

    @Override
    public XrQuaternion createQuaternionFromYaw(float yaw) {
        return XrQuaternionImpl.fromYaw(yaw);
    }

    @Override
    public XrPose createPose(XrVector3 translation, XrQuaternion rotation) {
        return XrPoseImpl.create(translation, rotation);
    }

    @Override
    public XrFloatSize3d createFloatSize3d(float width, float height, float depth) {
        return XrFloatSize3dImpl.create(width, height, depth);
    }

    @Override
    public XrSurfaceEntityHolder createSurfaceEntity(
            Object runtimeSession, @XrSurfaceEntityShape int shape) {
        assert runtimeSession instanceof Session;
        Session session = (Session) runtimeSession;
        Shape entityShape;
        switch (shape) {
            case XrSurfaceEntityShape.QUAD:
                entityShape = new Shape.Quad(new FloatSize2d(1f, 1f));
                break;
            case XrSurfaceEntityShape.SPHERE:
                entityShape = new Shape.Sphere(1f);
                break;
            case XrSurfaceEntityShape.HEMISPHERE:
                entityShape = new Shape.Hemisphere(1f);
                break;
            default:
                throw new IllegalArgumentException("Invalid shape: " + shape);
        }
        return XrSurfaceEntityHolderImpl.create(
                session,
                SurfaceEntity.create(
                        session,
                        Pose.Identity,
                        entityShape,
                        StereoMode.MONO,
                        SuperSampling.PENTAGON,
                        SurfaceProtection.NONE));
    }

    @Override
    public XrPanelEntityHolder createPanelEntity(Object runtimeSession, View view, String name) {
        assert runtimeSession instanceof Session;
        Session session = (Session) runtimeSession;
        return XrPanelEntityHolderImpl.create(
                session,
                PanelEntity.create(session, view, new FloatSize2d(1f, 1f), name, Pose.Identity));
    }
}
