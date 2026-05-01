// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import androidx.xr.runtime.Session;
import androidx.xr.runtime.math.Pose;
import androidx.xr.runtime.math.Quaternion;
import androidx.xr.runtime.math.Vector3;
import androidx.xr.scenecore.BaseEntity;
import androidx.xr.scenecore.Space;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrEntityHolder;

/** Base class for {@link XrEntityHolder} implementations. */
@NullMarked
public abstract class XrEntityHolderImpl<EntityType extends BaseEntity>
        implements XrEntityHolder<EntityType> {
    protected static final String TAG = "XrEntityHolderImpl";

    protected final Session mXrSession;
    protected final EntityType mEntity;
    protected boolean mIsDisposed;

    protected XrEntityHolderImpl(Session xrSession, EntityType entity) {
        mXrSession = xrSession;
        mEntity = entity;
    }

    @Override
    public EntityType getEntity() {
        return mEntity;
    }

    @Override
    public void setEntityPose(float[] translation) {
        assertDisposed();
        if (translation.length != 3) {
            throw new IllegalArgumentException("Translation must be length 3");
        }
        mEntity.setPose(
                new Pose(new Vector3(translation[0], translation[1], translation[2])),
                Space.ACTIVITY);
    }

    @Override
    public void setEntityPose(float[] translation, float[] rotation) {
        assertDisposed();
        if (translation.length != 3 || rotation.length != 4) {
            throw new IllegalArgumentException(
                    "Translation must be length 3 and rotation must be length 4");
        }
        mEntity.setPose(
                new Pose(
                        new Vector3(translation[0], translation[1], translation[2]),
                        new Quaternion(rotation[0], rotation[1], rotation[2], rotation[3])),
                Space.ACTIVITY);
    }

    @Override
    public float[] getEntityTranslation() {
        assertDisposed();
        Vector3 translation = mEntity.getPose(Space.ACTIVITY).getTranslation();
        return new float[] {translation.getX(), translation.getY(), translation.getZ()};
    }

    @Override
    public float[] getEntityRotation() {
        assertDisposed();
        Quaternion rotation = mEntity.getPose(Space.ACTIVITY).getRotation();
        return new float[] {rotation.getX(), rotation.getY(), rotation.getZ(), rotation.getW()};
    }

    @Override
    public float getEntityScale() {
        assertDisposed();
        return mEntity.getScale(Space.ACTIVITY);
    }

    @Override
    public void setEntityScale(float scale) {
        assertDisposed();
        mEntity.setScale(scale, Space.ACTIVITY);
    }

    @Override
    public void setEntityAlpha(float alpha) {
        assertDisposed();
        mEntity.setAlpha(alpha);
    }

    @Override
    public float getEntityAlpha() {
        assertDisposed();
        return mEntity.getAlpha(Space.ACTIVITY);
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
}
