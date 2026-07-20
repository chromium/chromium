// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import androidx.xr.runtime.math.FloatSize3d;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrFloatSize3d;

import java.util.Objects;

/** Implementation of {@link XrFloatSize3d} that wraps an AndroidX {@link FloatSize3d}. */
@NullMarked
public final class XrFloatSize3dImpl implements XrFloatSize3d {
    public static XrFloatSize3d create(float width, float height, float depth) {
        return new XrFloatSize3dImpl(width, height, depth);
    }

    public static FloatSize3d toFloatSize3d(XrFloatSize3d ext) {
        if (ext instanceof XrFloatSize3dImpl) {
            return ((XrFloatSize3dImpl) ext).getSize();
        }
        return new FloatSize3d(ext.getWidth(), ext.getHeight(), ext.getDepth());
    }

    public static XrFloatSize3d toXrFloatSize3d(FloatSize3d size) {
        return new XrFloatSize3dImpl(size);
    }

    private final FloatSize3d mSize;

    public XrFloatSize3dImpl(FloatSize3d size) {
        mSize = size;
    }

    public XrFloatSize3dImpl(float width, float height, float depth) {
        mSize = new FloatSize3d(width, height, depth);
    }

    @Override
    public float getWidth() {
        return mSize.getWidth();
    }

    @Override
    public float getHeight() {
        return mSize.getHeight();
    }

    @Override
    public float getDepth() {
        return mSize.getDepth();
    }

    public FloatSize3d getSize() {
        return mSize;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o instanceof XrFloatSize3dImpl) {
            return Objects.equals(mSize, ((XrFloatSize3dImpl) o).mSize);
        }
        if (o instanceof XrFloatSize3d) {
            XrFloatSize3d other = (XrFloatSize3d) o;
            return Float.compare(getWidth(), other.getWidth()) == 0
                    && Float.compare(getHeight(), other.getHeight()) == 0
                    && Float.compare(getDepth(), other.getDepth()) == 0;
        }
        return false;
    }

    @Override
    public int hashCode() {
        return mSize.hashCode();
    }

    @Override
    public String toString() {
        return String.format("XrFloatSize3dImpl(FloatSize3d=%s)", mSize);
    }
}
