// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.VisibleForTesting;
import androidx.xr.runtime.Session;
import androidx.xr.runtime.math.Vector3;
import androidx.xr.scenecore.BaseEntity;
import androidx.xr.scenecore.InputEvent;
import androidx.xr.scenecore.InputEvent.Action;
import androidx.xr.scenecore.InteractableComponent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.xr.scenecore.XrInteractableComponent;
import org.chromium.ui.xr.scenecore.XrVector3;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;

/** Implementation of {@link XrInteractableComponent}. */
@NullMarked
public class XrInteractableComponentImpl<EntityType extends BaseEntity>
        implements XrInteractableComponent {
    @FunctionalInterface
    private interface DragConsumer {
        void accept(OnDragListener listener, XrVector3 origin, XrVector3 direction);
    }

    private static final long CLICK_TIMEOUT_MS = 500;
    private static final float DRAG_START_DISTANCE_THRESHOLD_METERS = 0.1f; // 10cm
    // Chord length threshold between unit direction vectors (~4 degrees).
    // Note that Vector3.distance on unit vectors computes 2 * sin(angle / 2),
    // which approximates the angle in radians for small angles.
    private static final float DRAG_START_ANGLE_THRESHOLD_RADIANS = 0.07f; // ~4 degrees

    private final List<OnClickListener> mClickListeners = new ArrayList<>();
    private final List<OnDragListener> mDragListeners = new ArrayList<>();
    private final Session mXrSession;
    private final EntityType mEntity;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Runnable mClickTimeoutRunnable = () -> mWaitActionUp = false;
    private @Nullable InteractableComponent mInteractableComponent;
    private boolean mWaitActionUp;
    private boolean mWaitDragEnd;
    private @Nullable Vector3 mDownOrigin;
    private @Nullable Vector3 mDownDirection;

    public XrInteractableComponentImpl(Session xrSession, EntityType entity) {
        mXrSession = xrSession;
        mEntity = entity;
    }

    @Override
    public void setInteractable(boolean interactable) {
        detachFromEntity();

        if (interactable) {
            mInteractableComponent = InteractableComponent.create(mXrSession, this::onInputEvent);
            mEntity.addComponent(mInteractableComponent);
        }
    }

    @Override
    public void addOnClickListener(OnClickListener listener) {
        if (!mClickListeners.contains(listener)) {
            mClickListeners.add(listener);
        }
    }

    @Override
    public void removeOnClickListener(OnClickListener listener) {
        mClickListeners.remove(listener);
    }

    @Override
    public void addOnDragListener(OnDragListener listener) {
        if (!mDragListeners.contains(listener)) {
            mDragListeners.add(listener);
        }
    }

    @Override
    public void removeOnDragListener(OnDragListener listener) {
        mDragListeners.remove(listener);
    }

    @VisibleForTesting
    void onInputEvent(InputEvent event) {
        // Ignore events that hit child entities first.
        if (!event.getHitInfoList().isEmpty()
                && event.getHitInfoList().get(0).getInputEntity() != mEntity) {
            resetClickState();
            mWaitDragEnd = false;
            return;
        }

        Action action = event.getAction();
        boolean hasDragListeners = !mDragListeners.isEmpty();
        boolean hasClickListeners = !mClickListeners.isEmpty();

        if (action == Action.DOWN) {
            mDownOrigin = event.getOrigin();
            mDownDirection = event.getDirection();
            if (hasClickListeners) {
                mWaitActionUp = true;
                mHandler.postDelayed(mClickTimeoutRunnable, CLICK_TIMEOUT_MS);
            }
        } else if (action == Action.MOVE) {
            if (!mWaitDragEnd && isDragThresholdExceeded(event)) {
                if (hasDragListeners) {
                    mWaitDragEnd = true;
                    notifyDragListeners(event, OnDragListener::onDragStart);
                }
                resetClickState();
            }
            if (mWaitDragEnd) {
                notifyDragListeners(event, OnDragListener::onDragUpdate);
            }
        } else if (action == Action.UP) {
            if (mWaitDragEnd) {
                mWaitDragEnd = false;
                if (hasDragListeners) {
                    notifyDragListeners(event, OnDragListener::onDragEnd);
                }
            }
            if (hasClickListeners && mWaitActionUp) {
                notifyClickListeners(OnClickListener::onClick);
            }
            resetClickState();
        } else if (action == Action.CANCEL) {
            if (mWaitDragEnd) {
                mWaitDragEnd = false;
                if (hasDragListeners) {
                    notifyDragListeners(event, OnDragListener::onDragEnd);
                }
            }
            resetClickState();
        }
    }

    private void notifyDragListeners(InputEvent event, DragConsumer notification) {
        XrVector3 origin = new XrVector3Impl(event.getOrigin());
        XrVector3 direction = new XrVector3Impl(event.getDirection());
        for (OnDragListener listener : mDragListeners) {
            notification.accept(listener, origin, direction);
        }
    }

    private boolean isDragThresholdExceeded(InputEvent event) {
        if (mDownOrigin == null || mDownDirection == null) {
            return false;
        }
        float originDiff = Vector3.distance(event.getOrigin(), mDownOrigin);
        float directionDiff = Vector3.distance(event.getDirection(), mDownDirection);
        return originDiff > DRAG_START_DISTANCE_THRESHOLD_METERS
                || directionDiff > DRAG_START_ANGLE_THRESHOLD_RADIANS;
    }

    private void notifyClickListeners(Consumer<OnClickListener> notification) {
        for (OnClickListener listener : mClickListeners) {
            notification.accept(listener);
        }
    }

    private void resetClickState() {
        mHandler.removeCallbacks(mClickTimeoutRunnable);
        mWaitActionUp = false;
    }

    private void detachFromEntity() {
        if (mInteractableComponent != null) {
            mEntity.removeComponent(mInteractableComponent);
            mInteractableComponent = null;
        }
    }

    boolean hasOnClickListenerForTesting(OnClickListener listener) {
        return mClickListeners.contains(listener);
    }

    boolean hasOnDragListenerForTesting(OnDragListener listener) {
        return mDragListeners.contains(listener);
    }

    void dispose() {
        mClickListeners.clear();
        mDragListeners.clear();
        mHandler.removeCallbacks(mClickTimeoutRunnable);
        detachFromEntity();
    }
}
