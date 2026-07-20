// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import androidx.xr.runtime.math.Pose;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrPose;
import org.chromium.ui.xr.scenecore.XrQuaternion;
import org.chromium.ui.xr.scenecore.XrVector3;

import java.util.Objects;

/** Implementation of {@link XrPose} that wraps an AndroidX {@link Pose}. */
@NullMarked
public final class XrPoseImpl implements XrPose {
    public static XrPose create(XrVector3 translation, XrQuaternion rotation) {
        return new XrPoseImpl(translation, rotation);
    }

    public static Pose toPose(XrPose pose) {
        if (pose instanceof XrPoseImpl) {
            return ((XrPoseImpl) pose).getPose();
        }
        return new Pose(
                XrVector3Impl.toVector3(pose.getTranslation()),
                XrQuaternionImpl.toQuaternion(pose.getRotation()));
    }

    public static XrPose toXrPose(Pose pose) {
        return new XrPoseImpl(pose);
    }

    private final Pose mPose;

    public XrPoseImpl(Pose pose) {
        mPose = pose;
    }

    public XrPoseImpl(XrVector3 translation, XrQuaternion rotation) {
        mPose =
                new Pose(
                        XrVector3Impl.toVector3(translation),
                        XrQuaternionImpl.toQuaternion(rotation));
    }

    @Override
    public XrVector3 getTranslation() {
        return new XrVector3Impl(mPose.getTranslation());
    }

    @Override
    public XrQuaternion getRotation() {
        return new XrQuaternionImpl(mPose.getRotation());
    }

    public Pose getPose() {
        return mPose;
    }

    @Override
    public XrVector3 transformPoint(XrVector3 point) {
        return XrVector3Impl.toXrVector3(mPose.transformPoint(XrVector3Impl.toVector3(point)));
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o instanceof XrPoseImpl) {
            return Objects.equals(mPose, ((XrPoseImpl) o).mPose);
        }
        if (o instanceof XrPose) {
            XrPose other = (XrPose) o;
            return Objects.equals(getTranslation(), other.getTranslation())
                    && Objects.equals(getRotation(), other.getRotation());
        }
        return false;
    }

    @Override
    public int hashCode() {
        return mPose.hashCode();
    }

    @Override
    public String toString() {
        return String.format("XrPoseImpl(Pose=%s)", mPose);
    }
}
