// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.graphics.drawable.Drawable;
import android.support.annotation.IntDef;
import android.view.Surface;
import android.view.View;

import org.chromium.chrome.browser.compositor.CompositorSurfaceManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Provides the texture-backed surface used for drawing Android UI in VR to the CompositorView.
 * This class only partially fulfills the contract that the CompositorSurfaceManagerImpl fulfills.
 *
 * This class doesn't use SurfaceViews, as the texture-backed surface is drawn using GL during VR
 * scene compositing. This class also doesn't really manage the Surface in any way, and fakes the
 * standard Surface/View lifecycle so that the compositor remains unaware of this. The surface
 * format requested by the compositor is also ignored.
 */
public class VrCompositorSurfaceManager implements CompositorSurfaceManager {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({SurfaceState.NOT_REQUESTED, SurfaceState.REQUESTED, SurfaceState.PROVIDED})
    private @interface SurfaceState {
        int NOT_REQUESTED = 0;
        int REQUESTED = 1;
        int PROVIDED = 2;
    }

    private @SurfaceState int mSurfaceState = SurfaceState.NOT_REQUESTED;
    private Surface mSurface;
    private int mFormat;
    private int mWidth;
    private int mHeight;

    // Client that we notify about surface change events.
    private SurfaceManagerCallbackTarget mClient;

    public VrCompositorSurfaceManager(SurfaceManagerCallbackTarget client) {
        mClient = client;
    }

    /* package */ void setSurface(Surface surface, int format, int width, int height) {
        if (mSurfaceState == SurfaceState.PROVIDED) shutDown();
        mSurface = surface;
        mFormat = format;
        mWidth = width;
        mHeight = height;
        if (mSurfaceState == SurfaceState.REQUESTED) {
            mClient.surfaceCreated(mSurface);
            mClient.surfaceChanged(mSurface, mFormat, mWidth, mHeight);
            mSurfaceState = SurfaceState.PROVIDED;
        }
    }

    /* package */ void surfaceResized(int width, int height) {
        assert mSurface != null;
        mWidth = width;
        mHeight = height;
        if (mSurfaceState == SurfaceState.PROVIDED) {
            mClient.surfaceChanged(mSurface, mFormat, mWidth, mHeight);
        }
    }

    /* package */ void surfaceDestroyed() {
        shutDown();
        mSurface = null;
    }

    @Override
    public void shutDown() {
        if (mSurfaceState == SurfaceState.PROVIDED) mClient.surfaceDestroyed(mSurface);
        mSurfaceState = SurfaceState.NOT_REQUESTED;
    }

    @Override
    public void requestSurface(int format) {
        if (mSurface == null) {
            mSurfaceState = SurfaceState.REQUESTED;
            return;
        }
        if (mSurfaceState == SurfaceState.PROVIDED) shutDown();
        mClient.surfaceCreated(mSurface);
        mClient.surfaceChanged(mSurface, mFormat, mWidth, mHeight);
        mSurfaceState = SurfaceState.PROVIDED;
    }

    @Override
    public void doneWithUnownedSurface() {}

    @Override
    public void recreateSurface() {}

    @Override
    public void setBackgroundDrawable(Drawable background) {}

    @Override
    public void setWillNotDraw(boolean willNotDraw) {}

    @Override
    public void setVisibility(int visibility) {}

    @Override
    public View getActiveSurfaceView() {
        return null;
    }
}
