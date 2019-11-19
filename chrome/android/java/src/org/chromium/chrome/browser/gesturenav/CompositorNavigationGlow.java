// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.view.ViewGroup;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * Implements navigation glow using compositor layer for tab switcher or rendered web contents.
 */
@JNINamespace("android")
class CompositorNavigationGlow extends NavigationGlow {
    private float mAccumulatedScroll;
    private long mNativeNavigationGlow;

    /**
     * @pararm parentView Parent view where the glow view gets attached to.
     * @pararm webContents WebContents whose native view's cc layer will be used
     *        for rendering glow effect.
     * @return NavigationGlow object for rendered pages
     */
    public CompositorNavigationGlow(ViewGroup parentView, WebContents webContents) {
        super(parentView);
        mNativeNavigationGlow =
                CompositorNavigationGlowJni.get().init(CompositorNavigationGlow.this,
                        parentView.getResources().getDisplayMetrics().density, webContents);
    }

    @Override
    public void prepare(float startX, float startY) {
        CompositorNavigationGlowJni.get().prepare(mNativeNavigationGlow,
                CompositorNavigationGlow.this, startX, startY, mParentView.getWidth(),
                mParentView.getHeight());
    }

    @Override
    public void onScroll(float xDelta) {
        if (mNativeNavigationGlow == 0) return;
        mAccumulatedScroll += xDelta;
        CompositorNavigationGlowJni.get().onOverscroll(
                mNativeNavigationGlow, CompositorNavigationGlow.this, mAccumulatedScroll, xDelta);
    }

    @Override
    public void release() {
        if (mNativeNavigationGlow == 0) return;
        CompositorNavigationGlowJni.get().onOverscroll(
                mNativeNavigationGlow, CompositorNavigationGlow.this, 0, 0);
        mAccumulatedScroll = 0;
    }

    @Override
    public void reset() {
        if (mNativeNavigationGlow == 0) return;
        CompositorNavigationGlowJni.get().onReset(
                mNativeNavigationGlow, CompositorNavigationGlow.this);
        mAccumulatedScroll = 0;
    }

    @Override
    public void destroy() {
        if (mNativeNavigationGlow == 0) return;
        CompositorNavigationGlowJni.get().destroy(
                mNativeNavigationGlow, CompositorNavigationGlow.this);
        mNativeNavigationGlow = 0;
    }

    @NativeMethods
    interface Natives {
        long init(CompositorNavigationGlow caller, float dipScale, WebContents webContents);
        void prepare(long nativeNavigationGlow, CompositorNavigationGlow caller, float startX,
                float startY, int width, int height);
        void onOverscroll(long nativeNavigationGlow, CompositorNavigationGlow caller,
                float accumulatedScroll, float delta);
        void onReset(long nativeNavigationGlow, CompositorNavigationGlow caller);
        void destroy(long nativeNavigationGlow, CompositorNavigationGlow caller);
    }
}
