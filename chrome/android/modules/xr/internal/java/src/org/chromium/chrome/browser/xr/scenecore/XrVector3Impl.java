// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import androidx.xr.runtime.math.Vector3;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrVector3;

import java.util.Objects;

/** Implementation of {@link XrVector3} that wraps an AndroidX {@link Vector3}. */
@NullMarked
public final class XrVector3Impl implements XrVector3 {
    public static XrVector3 create(float x, float y, float z) {
        return new XrVector3Impl(x, y, z);
    }

    public static Vector3 toVector3(XrVector3 v) {
        if (v instanceof XrVector3Impl) {
            return ((XrVector3Impl) v).getVector();
        }
        return new Vector3(v.getX(), v.getY(), v.getZ());
    }

    public static XrVector3 toXrVector3(Vector3 v) {
        return new XrVector3Impl(v);
    }

    private final Vector3 mVector;

    public XrVector3Impl(Vector3 vector) {
        mVector = vector;
    }

    public XrVector3Impl(float x, float y, float z) {
        mVector = new Vector3(x, y, z);
    }

    @Override
    public float getX() {
        return mVector.getX();
    }

    @Override
    public float getY() {
        return mVector.getY();
    }

    @Override
    public float getZ() {
        return mVector.getZ();
    }

    public Vector3 getVector() {
        return mVector;
    }

    @Override
    public XrVector3 plus(XrVector3 other) {
        return toXrVector3(mVector.plus(toVector3(other)));
    }

    @Override
    public XrVector3 minus(XrVector3 other) {
        return toXrVector3(mVector.minus(toVector3(other)));
    }

    @Override
    public XrVector3 times(float scale) {
        return toXrVector3(mVector.times(scale));
    }

    @Override
    public float dot(XrVector3 other) {
        return mVector.dot(toVector3(other));
    }

    @Override
    public float getLength() {
        return mVector.getLength();
    }

    @Override
    public XrVector3 toNormalized() {
        return toXrVector3(mVector.toNormalized());
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o instanceof XrVector3Impl) {
            return Objects.equals(mVector, ((XrVector3Impl) o).mVector);
        }
        if (o instanceof XrVector3) {
            XrVector3 other = (XrVector3) o;
            return Float.compare(getX(), other.getX()) == 0
                    && Float.compare(getY(), other.getY()) == 0
                    && Float.compare(getZ(), other.getZ()) == 0;
        }
        return false;
    }

    @Override
    public int hashCode() {
        return mVector.hashCode();
    }

    @Override
    public String toString() {
        return String.format("XrVector3Impl(Vector3=%s)", mVector);
    }
}
