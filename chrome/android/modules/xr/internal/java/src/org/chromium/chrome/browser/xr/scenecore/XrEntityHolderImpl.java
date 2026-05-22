// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import androidx.xr.runtime.Session;
import androidx.xr.runtime.math.Pose;
import androidx.xr.runtime.math.Quaternion;
import androidx.xr.runtime.math.Vector3;
import androidx.xr.scenecore.BaseEntity;
import androidx.xr.scenecore.Entity;
import androidx.xr.scenecore.Space;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.xr.scenecore.XrEntityHolder;
import org.chromium.ui.xr.scenecore.XrSpace;

/** Base class for {@link XrEntityHolder} implementations. */
@NullMarked
public abstract class XrEntityHolderImpl<EntityType extends BaseEntity>
        implements XrEntityHolder<EntityType> {
    protected static final String TAG = "XrEntityHolderImpl";

    protected final Session mXrSession;
    protected final EntityType mEntity;
    protected boolean mIsDisposed;
    protected @Nullable XrEntityHolder<?> mParent;

    protected XrEntityHolderImpl(Session xrSession, EntityType entity) {
        mXrSession = xrSession;
        mEntity = entity;
    }

    private Space mapSpace(@XrSpace int space) {
        switch (space) {
            case XrSpace.ACTIVITY:
                return Space.ACTIVITY;
            case XrSpace.PARENT:
                return Space.PARENT;
            default:
                throw new IllegalArgumentException("Unknown XrSpace: " + space);
        }
    }

    @Override
    public EntityType getEntity() {
        return mEntity;
    }

    @Override
    public void setEntityPose(float[] translation, @XrSpace int space) {
        assertDisposed();
        if (translation.length != 3) {
            throw new IllegalArgumentException("Translation must be length 3");
        }
        mEntity.setPose(
                new Pose(new Vector3(translation[0], translation[1], translation[2])),
                mapSpace(space));
    }

    @Override
    public void setEntityPose(float[] translation, float[] rotation, @XrSpace int space) {
        assertDisposed();
        if (translation.length != 3 || rotation.length != 4) {
            throw new IllegalArgumentException(
                    "Translation must be length 3 and rotation must be length 4");
        }
        mEntity.setPose(
                new Pose(
                        new Vector3(translation[0], translation[1], translation[2]),
                        new Quaternion(rotation[0], rotation[1], rotation[2], rotation[3])),
                mapSpace(space));
    }

    @Override
    public float[] getEntityTranslation(@XrSpace int space) {
        assertDisposed();
        Vector3 translation = mEntity.getPose(mapSpace(space)).getTranslation();
        return new float[] {translation.getX(), translation.getY(), translation.getZ()};
    }

    @Override
    public float[] getEntityRotation(@XrSpace int space) {
        assertDisposed();
        Quaternion rotation = mEntity.getPose(mapSpace(space)).getRotation();
        return new float[] {rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW()};
    }

    @Override
    public float getEntityScale(@XrSpace int space) {
        assertDisposed();
        return mEntity.getScale(mapSpace(space));
    }

    @Override
    public void setEntityScale(float scale, @XrSpace int space) {
        assertDisposed();
        mEntity.setScale(scale, mapSpace(space));
    }

    @Override
    public void setEntityAlpha(float alpha) {
        assertDisposed();
        mEntity.setAlpha(alpha);
    }

    @Override
    public float getEntityAlpha(@XrSpace int space) {
        assertDisposed();
        return mEntity.getAlpha(mapSpace(space));
    }

    @Override
    public void setEntityEnabled(boolean enabled) {
        assertDisposed();
        mEntity.setEnabled(enabled);
    }

    @Override
    public boolean isEntityEnabled() {
        assertDisposed();
        return mEntity.isEnabled(/* includeParents= */ true);
    }

    @Override
    public void addChild(XrEntityHolder<?> child) {
        assertDisposed();
        child.setParent(this);
    }

    @Override
    public void setParent(@Nullable XrEntityHolder<?> parent) {
        assertDisposed();
        mParent = parent;
        if (parent == null) {
            mEntity.setParent(null);
        } else {
            assert parent.getEntity() instanceof Entity;
            mEntity.setParent((Entity) parent.getEntity());
        }
    }

    @Override
    public @Nullable XrEntityHolder<?> getParent() {
        assertDisposed();
        return mParent;
    }

    protected void assertDisposed() {
        if (mIsDisposed) {
            throw new IllegalStateException("Entity is already disposed");
        }
    }

    @Override
    public void dispose() {
        if (!mIsDisposed) {
            mEntity.dispose();
            mIsDisposed = true;
        }
    }

    @Override
    public boolean isDisposed() {
        return mIsDisposed;
    }
}
