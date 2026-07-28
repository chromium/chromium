// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import androidx.xr.runtime.math.Quaternion;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrQuaternion;
import org.chromium.ui.xr.scenecore.XrVector3;

import java.util.Objects;

/** Implementation of {@link XrQuaternion} that wraps an AndroidX {@link Quaternion}. */
@NullMarked
public final class XrQuaternionImpl implements XrQuaternion {
    public static XrQuaternion create(float x, float y, float z, float w) {
        return new XrQuaternionImpl(x, y, z, w);
    }

    public static XrQuaternion fromYaw(float yaw) {
        float sinHalf = (float) Math.sin(yaw / 2.0);
        float cosHalf = (float) Math.cos(yaw / 2.0);
        return new XrQuaternionImpl(0f, sinHalf, 0f, cosHalf);
    }

    public static Quaternion toQuaternion(XrQuaternion q) {
        if (q instanceof XrQuaternionImpl) {
            return ((XrQuaternionImpl) q).getQuaternion();
        }
        return new Quaternion(q.getX(), q.getY(), q.getZ(), q.getW());
    }

    public static XrQuaternion toXrQuaternion(Quaternion q) {
        return new XrQuaternionImpl(q);
    }

    private final Quaternion mQuaternion;

    public XrQuaternionImpl(Quaternion quaternion) {
        mQuaternion = quaternion;
    }

    public XrQuaternionImpl(float x, float y, float z, float w) {
        mQuaternion = new Quaternion(x, y, z, w);
    }

    @Override
    public float getX() {
        return mQuaternion.getX();
    }

    @Override
    public float getY() {
        return mQuaternion.getY();
    }

    @Override
    public float getZ() {
        return mQuaternion.getZ();
    }

    @Override
    public float getW() {
        return mQuaternion.getW();
    }

    @Override
    public float getYaw() {
        return (float)
                Math.atan2(
                        2f * (getW() * getY() + getZ() * getX()),
                        1f - 2f * (getY() * getY() + getZ() * getZ()));
    }

    public Quaternion getQuaternion() {
        return mQuaternion;
    }

    @Override
    public XrVector3 rotate(XrVector3 v) {
        return XrVector3Impl.toXrVector3(mQuaternion.times(XrVector3Impl.toVector3(v)));
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o instanceof XrQuaternionImpl) {
            return Objects.equals(mQuaternion, ((XrQuaternionImpl) o).mQuaternion);
        }
        if (o instanceof XrQuaternion) {
            XrQuaternion other = (XrQuaternion) o;
            return Float.compare(getX(), other.getX()) == 0
                    && Float.compare(getY(), other.getY()) == 0
                    && Float.compare(getZ(), other.getZ()) == 0
                    && Float.compare(getW(), other.getW()) == 0;
        }
        return false;
    }

    @Override
    public int hashCode() {
        return mQuaternion.hashCode();
    }

    @Override
    public String toString() {
        return String.format("XrQuaternionImpl(Quaternion=%s)", mQuaternion);
    }
}
