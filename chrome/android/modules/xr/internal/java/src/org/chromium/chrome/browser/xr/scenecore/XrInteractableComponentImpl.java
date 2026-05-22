// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.VisibleForTesting;
import androidx.xr.runtime.Session;
import androidx.xr.scenecore.BaseEntity;
import androidx.xr.scenecore.InputEvent;
import androidx.xr.scenecore.InteractableComponent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.xr.scenecore.XrInteractableComponent;

import java.util.ArrayList;
import java.util.List;

/** Implementation of {@link XrInteractableComponent}. */
@NullMarked
public class XrInteractableComponentImpl<EntityType extends BaseEntity>
        implements XrInteractableComponent {
    private static final long CLICK_TIMEOUT_MS = 500;

    private final List<OnClickListener> mClickListeners = new ArrayList<>();
    private final Session mXrSession;
    private final EntityType mEntity;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Runnable mClickTimeoutRunnable = () -> mWaitActionUp = false;
    private @Nullable InteractableComponent mInteractableComponent;
    private boolean mWaitActionUp;

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

    @VisibleForTesting
    void onInputEvent(InputEvent event) {
        if (mClickListeners.isEmpty()) return;

        // Ignore events that hit child entities first.
        if (!event.getHitInfoList().isEmpty()
                && event.getHitInfoList().get(0).getInputEntity() != mEntity) {
            resetClickState();
            return;
        }
        InputEvent.Action action = event.getAction();
        if (action == InputEvent.Action.DOWN) {
            mWaitActionUp = true;
            mHandler.postDelayed(mClickTimeoutRunnable, CLICK_TIMEOUT_MS);
        } else if (action == InputEvent.Action.UP) {
            if (mWaitActionUp) {
                for (OnClickListener listener : mClickListeners) {
                    listener.onClick();
                }
            }
            resetClickState();
        } else if (action == InputEvent.Action.CANCEL) {
            resetClickState();
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

    void dispose() {
        mClickListeners.clear();
        mHandler.removeCallbacks(mClickTimeoutRunnable);
        detachFromEntity();
    }
}
