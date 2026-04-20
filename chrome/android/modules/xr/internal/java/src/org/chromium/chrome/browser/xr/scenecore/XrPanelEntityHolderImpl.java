// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.xr.scenecore;

import android.util.Size;
import android.util.SizeF;

import androidx.xr.runtime.Session;
import androidx.xr.runtime.math.FloatSize2d;
import androidx.xr.runtime.math.IntSize2d;
import androidx.xr.scenecore.PanelEntity;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.xr.scenecore.XrMovableComponent;
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;
import org.chromium.ui.xr.scenecore.XrResizableComponent;

/**
 * Implementation of {@link XrPanelEntityHolder}.
 *
 * <p>TODO(crbug.com/495766632): Add test coverage for this implementation.
 */
@NullMarked
public class XrPanelEntityHolderImpl extends XrEntityHolderImpl<PanelEntity>
        implements XrPanelEntityHolder<PanelEntity> {
    private static final String TAG = "XrPanelEntityHolderImpl";

    private final XrMovableComponent mMovableComponent;
    private final XrResizableComponent mResizableComponent;

    public static XrPanelEntityHolderImpl create(Session xrSession, PanelEntity panelEntity) {
        return new XrPanelEntityHolderImpl(xrSession, panelEntity);
    }

    private XrPanelEntityHolderImpl(Session xrSession, PanelEntity panelEntity) {
        super(xrSession, panelEntity);
        mMovableComponent = new XrMovableComponentImpl<>(xrSession, panelEntity);
        mResizableComponent = new XrResizableComponentImpl<>(xrSession, panelEntity);
        mResizableComponent.addResizeListener(
                new XrResizableComponent.OnResizeListener() {
                    @Override
                    public void onResizeUpdate(float width, float height, float depth) {}

                    @Override
                    public void onResizeEnd(float width, float height, float depth) {
                        mEntity.setSize(new FloatSize2d(width, height));
                    }
                });
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
        FloatSize2d size = mEntity.getSize();
        return new SizeF(size.getWidth(), size.getHeight());
    }

    @Override
    public void setEntitySize(float width, float height) {
        assertDisposed();
        mEntity.setSize(new FloatSize2d(width, height));
    }

    @Override
    public Size getEntitySizeInPixels() {
        assertDisposed();
        IntSize2d size = mEntity.getSizeInPixels();
        return new Size(size.getWidth(), size.getHeight());
    }

    @Override
    public void setEntitySizeInPixels(int width, int height) {
        assertDisposed();
        mEntity.setSizeInPixels(new IntSize2d(width, height));
    }

    @Override
    public float getEntityCornerRadius() {
        assertDisposed();
        return mEntity.getCornerRadius();
    }

    @Override
    public void setEntityCornerRadius(float radius) {
        assertDisposed();
        mEntity.setCornerRadius(radius);
    }

    @Override
    public void dispose() {
        if (!mIsDisposed) {
            mMovableComponent.dispose();
            mResizableComponent.dispose();
            super.dispose();
        }
    }
}
