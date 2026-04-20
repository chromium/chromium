// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import android.util.SizeF;
import android.view.Surface;

import androidx.xr.runtime.Session;
import androidx.xr.runtime.math.FloatSize2d;
import androidx.xr.runtime.math.FloatSize3d;
import androidx.xr.runtime.math.IntSize2d;
import androidx.xr.scenecore.SurfaceEntity;
import androidx.xr.scenecore.SurfaceEntity.Shape;
import androidx.xr.scenecore.SurfaceEntity.StereoMode;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.xr.scenecore.XrCurvedSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrMovableComponent;
import org.chromium.ui.xr.scenecore.XrQuadSurfaceEntityHolder;
import org.chromium.ui.xr.scenecore.XrResizableComponent;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityHolder.Callback;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityShape;
import org.chromium.ui.xr.scenecore.XrSurfaceEntityStereoMode;

import java.util.Map;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * Implementation of {@link XrQuadSurfaceEntityHolder} and {@link XrCurvedSurfaceEntityHolder}.
 *
 * <p>TODO(crbug.com/495766632): Add test coverage for this implementation.
 */
@NullMarked
public class XrSurfaceEntityHolderImpl extends XrEntityHolderImpl<SurfaceEntity>
        implements XrQuadSurfaceEntityHolder<SurfaceEntity>,
                XrCurvedSurfaceEntityHolder<SurfaceEntity> {
    protected static final String TAG = "XrSurfaceEntityHolderImpl";
    protected static final Map<Integer, StereoMode> STEREO_MODE_MAP =
            Map.of(
                    XrSurfaceEntityStereoMode.MONO, StereoMode.MONO,
                    XrSurfaceEntityStereoMode.MULTIVIEW_LEFT_PRIMARY,
                            StereoMode.MULTIVIEW_LEFT_PRIMARY,
                    XrSurfaceEntityStereoMode.MULTIVIEW_RIGHT_PRIMARY,
                            StereoMode.MULTIVIEW_RIGHT_PRIMARY,
                    XrSurfaceEntityStereoMode.SIDE_BY_SIDE, StereoMode.SIDE_BY_SIDE,
                    XrSurfaceEntityStereoMode.TOP_BOTTOM, StereoMode.TOP_BOTTOM);

    private final CopyOnWriteArrayList<Callback> mCallbacks = new CopyOnWriteArrayList<>();
    private IntSize2d mCurrentSurfaceDimensions = new IntSize2d(1, 1);
    private final XrMovableComponent mMovableComponent;
    private final XrResizableComponent mResizableComponent;

    public static XrSurfaceEntityHolderImpl create(Session xrSession, SurfaceEntity surfaceEntity) {
        return new XrSurfaceEntityHolderImpl(xrSession, surfaceEntity);
    }

    protected XrSurfaceEntityHolderImpl(Session xrSession, SurfaceEntity surfaceEntity) {
        super(xrSession, surfaceEntity);
        mMovableComponent = new XrMovableComponentImpl<>(xrSession, surfaceEntity);
        mResizableComponent = new XrResizableComponentImpl<>(xrSession, surfaceEntity);
        mResizableComponent.addResizeListener(
                new XrResizableComponent.OnResizeListener() {
                    @Override
                    public void onResizeUpdate(float width, float height, float depth) {}

                    @Override
                    public void onResizeEnd(float width, float height, float depth) {
                        if (mEntity.getShape() instanceof Shape.Quad) {
                            mEntity.setShape(new Shape.Quad(new FloatSize2d(width, height)));
                        }
                    }
                });
    }

    @Override
    public void addCallback(Callback callback) {
        if (!mCallbacks.contains(callback)) {
            mCallbacks.add(callback);

            Surface surface = getSurface();
            if (surface != null && surface.isValid()) {
                callback.surfaceCreated(surface);
                callback.surfaceChanged(
                        surface,
                        mCurrentSurfaceDimensions.getWidth(),
                        mCurrentSurfaceDimensions.getHeight());
            }
        }
    }

    private void notifySurfaceChanged() {
        Surface surface = getSurface();
        if (surface != null && surface.isValid()) {
            for (Callback callback : mCallbacks) {
                callback.surfaceChanged(
                        surface,
                        mCurrentSurfaceDimensions.getWidth(),
                        mCurrentSurfaceDimensions.getHeight());
            }
        }
    }

    private void notifySurfaceDestroyed() {
        for (Callback callback : mCallbacks) {
            callback.surfaceDestroyed();
        }
    }

    @Override
    public void removeCallback(Callback callback) {
        mCallbacks.remove(callback);
    }

    @Override
    public @Nullable Surface getSurface() {
        assertDisposed();
        return mEntity.getSurface();
    }

    @Override
    public void setSurfacePixelDimensions(int width, int height) {
        assertDisposed();
        mCurrentSurfaceDimensions = new IntSize2d(width, height);
        mEntity.setSurfacePixelDimensions(mCurrentSurfaceDimensions);
        notifySurfaceChanged();
    }

    @Override
    public @XrSurfaceEntityStereoMode int getSurfaceStereoMode() {
        assertDisposed();
        StereoMode surfaceStereoMode = mEntity.getStereoMode();
        for (Map.Entry<Integer, StereoMode> entry : STEREO_MODE_MAP.entrySet()) {
            if (entry.getValue().equals(surfaceStereoMode)) return entry.getKey();
        }
        throw new IllegalStateException("Unknown stereo mode: " + surfaceStereoMode);
    }

    @Override
    public void setSurfaceStereoMode(@XrSurfaceEntityStereoMode int stereoMode) {
        assertDisposed();
        StereoMode surfaceStereoMode = STEREO_MODE_MAP.get(stereoMode);
        if (surfaceStereoMode != null) {
            mEntity.setStereoMode(surfaceStereoMode);
        } else {
            throw new IllegalArgumentException("Invalid stereo mode: " + stereoMode);
        }
    }

    @Override
    public @XrSurfaceEntityShape int getSurfaceShape() {
        assertDisposed();
        if (mEntity.getShape() instanceof Shape.Quad) {
            return XrSurfaceEntityShape.QUAD;
        } else if (mEntity.getShape() instanceof Shape.Sphere) {
            return XrSurfaceEntityShape.SPHERE;
        } else if (mEntity.getShape() instanceof Shape.Hemisphere) {
            return XrSurfaceEntityShape.HEMISPHERE;
        } else {
            throw new IllegalStateException("Unknown surface shape: " + mEntity.getShape());
        }
    }

    @Override
    public void setSurfaceShape(@XrSurfaceEntityShape int shape) {
        assertDisposed();
        switch (shape) {
            case XrSurfaceEntityShape.QUAD:
                mEntity.setShape(new Shape.Quad(new FloatSize2d(1f, 1f)));
                break;
            case XrSurfaceEntityShape.SPHERE:
                mEntity.setShape(new Shape.Sphere(1f));
                break;
            case XrSurfaceEntityShape.HEMISPHERE:
                mEntity.setShape(new Shape.Hemisphere(1f));
                break;
            default:
                throw new IllegalArgumentException("Invalid surface shape: " + shape);
        }
    }

    @Override
    public XrMovableComponent getMovableComponent() {
        return mMovableComponent;
    }

    @Override
    public XrResizableComponent getResizableComponent() {
        return mResizableComponent;
    }

    @Override
    public SizeF getEntitySize() {
        assertDisposed();
        FloatSize3d dimensions = mEntity.getDimensions();
        return new SizeF(dimensions.getWidth(), dimensions.getHeight());
    }

    @Override
    public void setEntitySize(float width, float height) {
        assertDisposed();
        if (mEntity.getShape() instanceof Shape.Quad) {
            mEntity.setShape(new Shape.Quad(new FloatSize2d(width, height)));
        }
    }

    @Override
    public float getEntityRadius() {
        assertDisposed();
        if (mEntity.getShape() instanceof Shape.Sphere) {
            return ((Shape.Sphere) mEntity.getShape()).getRadius();
        } else if (mEntity.getShape() instanceof Shape.Hemisphere) {
            return ((Shape.Hemisphere) mEntity.getShape()).getRadius();
        }
        return 0f;
    }

    @Override
    public void setEntityRadius(float radius) {
        assertDisposed();
        if (mEntity.getShape() instanceof Shape.Sphere) {
            mEntity.setShape(new Shape.Sphere(radius));
        } else if (mEntity.getShape() instanceof Shape.Hemisphere) {
            mEntity.setShape(new Shape.Hemisphere(radius));
        }
    }

    @Override
    public void dispose() {
        if (!mIsDisposed) {
            notifySurfaceDestroyed();
            mCallbacks.clear();
            mMovableComponent.dispose();
            mResizableComponent.dispose();
            super.dispose();
        }
    }
}
