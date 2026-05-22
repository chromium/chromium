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
import org.chromium.ui.xr.scenecore.XrPanelEntityHolder;

/** Implementation of {@link XrPanelEntityHolder}. */
@NullMarked
public class XrPanelEntityHolderImpl extends XrTransformableEntityHolderImpl<PanelEntity>
        implements XrPanelEntityHolder<PanelEntity> {
    private static final String TAG = "XrPanelEntityHolderImpl";

    public static XrPanelEntityHolderImpl create(Session xrSession, PanelEntity panelEntity) {
        return new XrPanelEntityHolderImpl(xrSession, panelEntity);
    }

    private XrPanelEntityHolderImpl(Session xrSession, PanelEntity panelEntity) {
        super(xrSession, panelEntity);
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
        if (width <= 0f || height <= 0f) return;
        mEntity.setSize(new FloatSize2d(width, height));
        mMovableComponent.setSize(width, height, 0f);

        if (mResizableComponent.shouldMaintainAspectRatio()) {
            float aspectRatio = width / height;
            float[] minSize = mResizableComponent.getMinSize();
            float[] maxSize = mResizableComponent.getMaxSize();
            mResizableComponent.setMinSize(minSize[0], minSize[0] / aspectRatio, minSize[2]);
            mResizableComponent.setMaxSize(maxSize[0], maxSize[0] / aspectRatio, maxSize[2]);
        }
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
        if (width <= 0 || height <= 0) return;
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
}
