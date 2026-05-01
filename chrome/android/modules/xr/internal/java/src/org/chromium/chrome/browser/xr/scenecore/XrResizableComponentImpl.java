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
    private FloatSize3d mMinEntitySize = new FloatSize3d(0f, 0f, 1f);
    private FloatSize3d mMaxEntitySize = new FloatSize3d(10f, 10f, 1f);
    private final Session mXrSession;
    private final EntityType mEntity;

    public XrResizableComponentImpl(Session xrSession, EntityType entity) {
        mXrSession = xrSession;
        mEntity = entity;
    }

    @Override
    public void setMinSize(float width, float height) {
        mMinEntitySize = new FloatSize3d(width, height, 1f);
        if (mResizableComponent != null) {
            mResizableComponent.setMinimumEntitySize(mMinEntitySize);
        }
    }

    @Override
    public void setMaxSize(float width, float height) {
        mMaxEntitySize = new FloatSize3d(width, height, 1f);
        if (mResizableComponent != null) {
            mResizableComponent.setMaximumEntitySize(mMaxEntitySize);
        }
    }

    @Override
    public void setResizable(boolean resizable, boolean maintainAspectRatio) {
        detachFromEntity();

        if (resizable) {
            mResizableComponent =
                    ResizableComponent.create(
                            mXrSession,
                            mMinEntitySize,
                            mMaxEntitySize,
                            /* executor= */ ThreadUtils.getUiThreadHandler()::post,
                            /* resizeEventListener= */ (event) -> {});
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
            mResizeListenersMap.put(
                    listener,
                    new Consumer<ResizeEvent>() {
                        @Override
                        public void accept(ResizeEvent event) {
                            FloatSize3d size = event.getNewSize();
                            if (event.getResizeState() == ResizeEvent.ResizeState.START) {
                                listener.onResizeStart(
                                        size.getWidth(), size.getHeight(), size.getDepth());
                            } else if (event.getResizeState() == ResizeEvent.ResizeState.ONGOING) {
                                listener.onResizeUpdate(
                                        size.getWidth(), size.getHeight(), size.getDepth());
                            } else if (event.getResizeState() == ResizeEvent.ResizeState.END) {
                                listener.onResizeEnd(
                                        size.getWidth(), size.getHeight(), size.getDepth());
                            }
                        }
                    });
            if (mResizableComponent != null) {
                mResizableComponent.addResizeEventListener(mResizeListenersMap.get(listener));
            }
        }
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

    @Override
    public void dispose() {
        mResizeListenersMap.clear();
        detachFromEntity();
    }
}
