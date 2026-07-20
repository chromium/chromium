// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import androidx.xr.runtime.Session;
import androidx.xr.runtime.math.FloatSize3d;
import androidx.xr.scenecore.BaseEntity;
import androidx.xr.scenecore.ResizableComponent;
import androidx.xr.scenecore.ResizeEvent;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.xr.scenecore.XrFloatSize3d;
import org.chromium.ui.xr.scenecore.XrResizableComponent;

import java.util.HashMap;
import java.util.Map;
import java.util.function.Consumer;

/** Implementation of {@link XrResizableComponent}. */
@NullMarked
public class XrResizableComponentImpl<EntityType extends BaseEntity>
        implements XrResizableComponent {
    private final Map<OnResizeListener, Consumer<ResizeEvent>> mResizeListenersMap =
            new HashMap<>();
    private @Nullable ResizableComponent mResizableComponent;
    private FloatSize3d mMinEntitySize = new FloatSize3d(0f, 0f, 0f);
    private FloatSize3d mMaxEntitySize = new FloatSize3d(10f, 10f, 0f);
    private boolean mMaintainAspectRatio;
    private final Session mXrSession;
    private final EntityType mEntity;
    private final @Nullable OnResizeListener mCustomResizeListener;

    public XrResizableComponentImpl(Session xrSession, EntityType entity) {
        this(xrSession, entity, null);
    }

    public XrResizableComponentImpl(
            Session xrSession, EntityType entity, @Nullable OnResizeListener customResizeListener) {
        mXrSession = xrSession;
        mEntity = entity;
        mCustomResizeListener = customResizeListener;
    }

    @Override
    public void setMinSize(XrFloatSize3d size) {
        mMinEntitySize = XrFloatSize3dImpl.toFloatSize3d(size);
        if (mResizableComponent != null) {
            mResizableComponent.setMinimumEntitySize(mMinEntitySize);
        }
    }

    @Override
    public XrFloatSize3d getMinSize() {
        return XrFloatSize3dImpl.toXrFloatSize3d(mMinEntitySize);
    }

    @Override
    public void setMaxSize(XrFloatSize3d size) {
        mMaxEntitySize = XrFloatSize3dImpl.toFloatSize3d(size);
        if (mResizableComponent != null) {
            mResizableComponent.setMaximumEntitySize(mMaxEntitySize);
        }
    }

    @Override
    public XrFloatSize3d getMaxSize() {
        return XrFloatSize3dImpl.toXrFloatSize3d(mMaxEntitySize);
    }

    @Override
    public void setResizable(boolean resizable, boolean maintainAspectRatio) {
        mMaintainAspectRatio = maintainAspectRatio;
        detachFromEntity();

        if (resizable) {
            Consumer<ResizeEvent> resizeEventListener =
                    mCustomResizeListener != null
                            ? convertToResizeEventListener(mCustomResizeListener)
                            : (event) -> {};
            mResizableComponent =
                    ResizableComponent.create(
                            mXrSession,
                            mMinEntitySize,
                            mMaxEntitySize,
                            ThreadUtils.getUiThreadHandler()::post,
                            resizeEventListener);
            mResizableComponent.setFixedAspectRatioEnabled(maintainAspectRatio);
            for (Consumer<ResizeEvent> listener : mResizeListenersMap.values()) {
                mResizableComponent.addResizeEventListener(listener);
            }
            mEntity.addComponent(mResizableComponent);
        }
    }

    @Override
    public void addResizeListener(OnResizeListener listener) {
        if (!mResizeListenersMap.containsKey(listener)) {
            mResizeListenersMap.put(listener, convertToResizeEventListener(listener));
            if (mResizableComponent != null) {
                mResizableComponent.addResizeEventListener(mResizeListenersMap.get(listener));
            }
        }
    }

    private Consumer<ResizeEvent> convertToResizeEventListener(OnResizeListener listener) {
        return new Consumer<ResizeEvent>() {
            @Override
            public void accept(ResizeEvent event) {
                XrFloatSize3d size = XrFloatSize3dImpl.toXrFloatSize3d(event.getNewSize());
                if (event.getResizeState() == ResizeEvent.ResizeState.START) {
                    listener.onResizeStart(size);
                } else if (event.getResizeState() == ResizeEvent.ResizeState.ONGOING) {
                    listener.onResizeUpdate(size);
                } else if (event.getResizeState() == ResizeEvent.ResizeState.END) {
                    listener.onResizeEnd(size);
                }
            }
        };
    }

    @Override
    public void removeResizeListener(OnResizeListener listener) {
        if (mResizableComponent != null && mResizeListenersMap.containsKey(listener)) {
            mResizableComponent.removeResizeEventListener(mResizeListenersMap.get(listener));
            mResizeListenersMap.remove(listener);
        }
    }

    public boolean hasResizeListenerForTesting(OnResizeListener listener) {
        return mResizeListenersMap.containsKey(listener);
    }

    private void detachFromEntity() {
        if (mResizableComponent != null) {
            mEntity.removeComponent(mResizableComponent);
            mResizableComponent = null;
        }
    }

    boolean shouldMaintainAspectRatio() {
        return mMaintainAspectRatio;
    }

    void dispose() {
        mResizeListenersMap.clear();
        detachFromEntity();
    }
}
