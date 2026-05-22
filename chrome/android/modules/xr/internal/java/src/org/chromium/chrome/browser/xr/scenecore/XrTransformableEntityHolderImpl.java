// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import androidx.xr.runtime.Session;
import androidx.xr.scenecore.BaseEntity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrInteractableComponent;
import org.chromium.ui.xr.scenecore.XrMovableComponent;
import org.chromium.ui.xr.scenecore.XrResizableComponent;
import org.chromium.ui.xr.scenecore.XrTransformableEntityHolder;

/**
 * Base class for XR entity holders that support transformations (movement, resizing) and
 * interactions.
 *
 * @param <EntityType> The type of the underlying XR entity.
 */
@NullMarked
public abstract class XrTransformableEntityHolderImpl<EntityType extends BaseEntity>
        extends XrEntityHolderImpl<EntityType> implements XrTransformableEntityHolder<EntityType> {

    protected final XrInteractableComponentImpl<EntityType> mInteractableComponent;
    protected final XrMovableComponentImpl<EntityType> mMovableComponent;
    protected final XrResizableComponentImpl<EntityType> mResizableComponent;

    private final XrResizableComponent.OnResizeListener mResizeListener =
            new XrResizableComponent.OnResizeListener() {
                @Override
                public void onResizeUpdate(float width, float height, float depth) {}

                @Override
                public void onResizeEnd(float width, float height, float depth) {
                    setEntitySize(width, height);
                }
            };

    protected XrTransformableEntityHolderImpl(Session xrSession, EntityType entity) {
        super(xrSession, entity);
        mInteractableComponent = new XrInteractableComponentImpl<>(xrSession, entity);
        mMovableComponent = new XrMovableComponentImpl<>(xrSession, entity);
        mResizableComponent = new XrResizableComponentImpl<>(xrSession, entity, mResizeListener);
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
    public XrInteractableComponent getInteractableComponent() {
        return mInteractableComponent;
    }

    @Override
    public void dispose() {
        if (!mIsDisposed) {
            mMovableComponent.dispose();
            mResizableComponent.dispose();
            mInteractableComponent.dispose();
            super.dispose();
        }
    }
}
