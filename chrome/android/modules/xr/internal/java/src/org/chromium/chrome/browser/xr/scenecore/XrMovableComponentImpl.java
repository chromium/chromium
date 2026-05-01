// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import androidx.xr.runtime.Session;
import androidx.xr.runtime.math.Pose;
import androidx.xr.runtime.math.Quaternion;
import androidx.xr.runtime.math.Ray;
import androidx.xr.runtime.math.Vector3;
import androidx.xr.scenecore.BaseEntity;
import androidx.xr.scenecore.Entity;
import androidx.xr.scenecore.EntityMoveListener;
import androidx.xr.scenecore.MovableComponent;

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
    private @Nullable MovableComponent mMovableComponent;

    public XrMovableComponentImpl(Session xrSession, EntityType entity) {
        mXrSession = xrSession;
        mEntity = entity;
    }

    @Override
    public void setMovable(boolean movable, boolean scaleInZ) {
        detachFromEntity();

        if (movable) {
            mMovableComponent = MovableComponent.createSystemMovable(mXrSession, scaleInZ);
            for (EntityMoveListener listener : mMoveListenersMap.values()) {
                mMovableComponent.addMoveListener(listener);
            }
            mEntity.addComponent(mMovableComponent);
        }
    }

    @Override
    public void addMoveListener(OnMoveListener listener) {
        if (!mMoveListenersMap.containsKey(listener)) {
            mMoveListenersMap.put(
                    listener,
                    new EntityMoveListener() {
                        @Override
                        public void onMoveStart(
                                Entity entity,
                                Ray initialInputRay,
                                Pose initialPose,
                                float initialScale,
                                Entity initialParent) {
                            Vector3 translation = initialPose.getTranslation();
                            Quaternion rotation = initialPose.getRotation();
                            listener.onMoveStart(
                                    new float[] {
                                        translation.getX(), translation.getY(), translation.getZ()
                                    },
                                    new float[] {
                                        rotation.getX(),
                                        rotation.getY(),
                                        rotation.getZ(),
                                        rotation.getW()
                                    },
                                    initialScale);
                        }

                        @Override
                        public void onMoveUpdate(
                                Entity entity,
                                Ray currentInputRay,
                                Pose currentPose,
                                float currentScale) {
                            Vector3 translation = currentPose.getTranslation();
                            Quaternion rotation = currentPose.getRotation();
                            listener.onMoveUpdate(
                                    new float[] {
                                        translation.getX(), translation.getY(), translation.getZ()
                                    },
                                    new float[] {
                                        rotation.getX(),
                                        rotation.getY(),
                                        rotation.getZ(),
                                        rotation.getW()
                                    },
                                    currentScale);
                        }

                        @Override
                        public void onMoveEnd(
                                Entity entity,
                                Ray finalInputRay,
                                Pose finalPose,
                                float finalScale,
                                @Nullable Entity updatedParent) {
                            Vector3 translation = finalPose.getTranslation();
                            Quaternion rotation = finalPose.getRotation();
                            listener.onMoveEnd(
                                    new float[] {
                                        translation.getX(), translation.getY(), translation.getZ()
                                    },
                                    new float[] {
                                        rotation.getX(),
                                        rotation.getY(),
                                        rotation.getZ(),
                                        rotation.getW()
                                    },
                                    finalScale);
                        }
                    });
            if (mMovableComponent != null) {
                mMovableComponent.addMoveListener(mMoveListenersMap.get(listener));
            }
        }
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

    @Override
    public void dispose() {
        mMoveListenersMap.clear();
        detachFromEntity();
    }
}
