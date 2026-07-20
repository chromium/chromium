// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import androidx.xr.runtime.Session;
import androidx.xr.runtime.math.FloatSize3d;
import androidx.xr.runtime.math.Pose;
import androidx.xr.runtime.math.Ray;
import androidx.xr.scenecore.BaseEntity;
import androidx.xr.scenecore.Entity;
import androidx.xr.scenecore.EntityMoveListener;
import androidx.xr.scenecore.MovableComponent;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.xr.scenecore.XrMovableComponent;

import java.util.HashMap;
import java.util.Map;

/** Implementation of {@link XrMovableComponent}. */
@NullMarked
public class XrMovableComponentImpl<EntityType extends BaseEntity> implements XrMovableComponent {
    private final Map<OnMoveListener, EntityMoveListener> mMoveListenersMap = new HashMap<>();
    private final Session mXrSession;
    private final EntityType mEntity;
    private @Nullable OnMoveListener mCustomMoveHandler;
    private @Nullable MovableComponent mMovableComponent;
    private @Nullable FloatSize3d mLastSetSize;

    public XrMovableComponentImpl(Session xrSession, EntityType entity) {
        mXrSession = xrSession;
        mEntity = entity;
    }

    @Override
    public void setMovable(boolean movable, boolean scaleInZ) {
        updateMovableComponent(movable, scaleInZ);
    }

    @Override
    public void setCustomMoveHandler(@Nullable OnMoveListener customMoveHandler) {
        mCustomMoveHandler = customMoveHandler;
    }

    private void updateMovableComponent(boolean movable, boolean scaleInZ) {
        detachFromEntity();

        if (movable) {
            if (mCustomMoveHandler != null) {
                mMovableComponent =
                        MovableComponent.createCustomMovable(
                                mXrSession,
                                scaleInZ,
                                ThreadUtils.getUiThreadHandler()::post,
                                convertToEntityMoveListener(mCustomMoveHandler));
            } else {
                mMovableComponent = MovableComponent.createSystemMovable(mXrSession, scaleInZ);
            }
            for (EntityMoveListener listener : mMoveListenersMap.values()) {
                mMovableComponent.addMoveListener(listener);
            }
            mEntity.addComponent(mMovableComponent);
        }
    }

    @Override
    public void addMoveListener(OnMoveListener listener) {
        if (!mMoveListenersMap.containsKey(listener)) {
            mMoveListenersMap.put(listener, convertToEntityMoveListener(listener));
            if (mMovableComponent != null) {
                mMovableComponent.addMoveListener(mMoveListenersMap.get(listener));
            }
        }
    }

    private EntityMoveListener convertToEntityMoveListener(OnMoveListener listener) {
        return new EntityMoveListener() {
            @Override
            public void onMoveStart(
                    Entity entity,
                    Ray initialInputRay,
                    Pose initialPose,
                    float initialScale,
                    Entity initialParent) {
                listener.onMoveStart(XrPoseImpl.toXrPose(initialPose), initialScale);
            }

            @Override
            public void onMoveUpdate(
                    Entity entity, Ray currentInputRay, Pose currentPose, float currentScale) {
                listener.onMoveUpdate(XrPoseImpl.toXrPose(currentPose), currentScale);
            }

            @Override
            public void onMoveEnd(
                    Entity entity,
                    Ray finalInputRay,
                    Pose finalPose,
                    float finalScale,
                    @Nullable Entity updatedParent) {
                listener.onMoveEnd(XrPoseImpl.toXrPose(finalPose), finalScale);
            }
        };
    }

    @Override
    public void removeMoveListener(OnMoveListener listener) {
        if (mMovableComponent != null && mMoveListenersMap.containsKey(listener)) {
            mMovableComponent.removeMoveListener(mMoveListenersMap.get(listener));
            mMoveListenersMap.remove(listener);
        }
    }

    public boolean hasMoveListenerForTesting(OnMoveListener listener) {
        return mMoveListenersMap.containsKey(listener);
    }

    private void detachFromEntity() {
        if (mMovableComponent != null) {
            mEntity.removeComponent(mMovableComponent);
            mMovableComponent = null;
        }
    }

    void setSize(float width, float height, float depth) {
        mLastSetSize = new FloatSize3d(width, height, depth);
        if (mMovableComponent != null) {
            mMovableComponent.setSize(mLastSetSize);
        }
    }

    @Nullable FloatSize3d getLastSetSizeForTesting() {
        return mLastSetSize;
    }

    @Nullable OnMoveListener getCustomMoveHandlerForTesting() {
        return mCustomMoveHandler;
    }

    void dispose() {
        mMoveListenersMap.clear();
        detachFromEntity();
    }
}
