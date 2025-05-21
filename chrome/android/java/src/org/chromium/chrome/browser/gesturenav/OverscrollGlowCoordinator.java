// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.ui.base.WindowAndroid;

/** Coordinator of controlling overscroll glow effect. */
@NullMarked
public class OverscrollGlowCoordinator {

    // {@link NavigationGlow} object for rendered pages.
    private final OverscrollGlowOverlay mCompositorGlowEffect;
    // The x coordinator of initial gesture.
    private float mStartX;

    public OverscrollGlowCoordinator(
            WindowAndroid window,
            LayoutManager layoutManager,
            ViewGroup compositorParentView,
            Runnable requestRunnable) {
        mCompositorGlowEffect =
                new OverscrollGlowOverlay(window, compositorParentView, requestRunnable);
        layoutManager.addSceneOverlay(mCompositorGlowEffect);
    }

    /**
     * Start showing edge glow effect.
     *
     * @param x Current x coordinator of the touch event.
     * @param y Current y coordinator of the touch event.
     */
    public void showGlow(float x, float y) {
        mStartX = x;
        getGlowEffect().prepare(x, y);
    }

    /**
     * Signals a pull update for glow effect.
     *
     * @param x Current x coordinator of the touch event.
     * @param y Current y coordinator of the touch event.
     */
    public void pullGlow(float x, float y) {
        getGlowEffect().onScroll(x - mStartX);
    }

    /** Release the glow effect. */
    public void releaseGlow() {
        getGlowEffect().release();
    }

    /** Reset the glow effect. */
    public void resetGlow() {
        getGlowEffect().reset();
    }

    public boolean isShowing() {
        return getGlowEffect().isShowing();
    }

    private NavigationGlow getGlowEffect() {
        return mCompositorGlowEffect;
    }

    public static Class getSceneOverlayClass() {
        return OverscrollGlowOverlay.class;
    }
}
