// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.os.Build;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.View;
import android.view.ViewTreeObserver.OnPreDrawListener;
import android.widget.FrameLayout;

import org.chromium.base.TraceEvent;

/**
 * Holds views that will be drawn into a texture when in VR. Has no effect outside of VR.
 */
public class VrViewContainer extends FrameLayout {
    private Surface mSurface;
    private OnPreDrawListener mPredrawListener;

    /**
     * See {@link FrameLayout#FrameLayout(Context)}.
     */
    public VrViewContainer(Context context) {
        super(context);
        // We need a pre-draw listener to invalidate the native page because scrolling usually
        // doesn't trigger an onDraw call, so our texture won't get updated.
        mPredrawListener = new OnPreDrawListener() {
            @Override
            public boolean onPreDraw() {
                // Since we're drawing to a texture we have to fully invalidate the view or we won't
                // pick up things like scrolling and will encounter tons of rendering artifacts.
                if (isDirty()) invalidate();
                return true;
            }
        };
        getViewTreeObserver().addOnPreDrawListener(mPredrawListener);
        setBackgroundColor(Color.TRANSPARENT);
    }

    // We want screen taps to only be routed to the GvrUiLayout, and not propagate through to the
    // java views. So screen taps are ignored here and received by the GvrUiLayout.
    // Touch events generated from the VR controller are injected directly into this View's
    // child.
    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        return false;
    }

    // We also avoid propagation of hover events from the android root view to the java views.
    @Override
    public boolean dispatchHoverEvent(MotionEvent event) {
        return false;
    }

    @SuppressLint("MissingSuperCall")
    @Override
    public void draw(Canvas canvas) {
        if (mSurface == null) return;
        try (TraceEvent e = TraceEvent.scoped("VrViewContainer.dispatchDraw")) {
            // The linter doesn't understand O_MR1+, so we need this line here to prevent the
            // linter from complaining about lockHardwareCanvas. This won't be reached pre-N
            // anyways.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) return;
            // This should be replaced with using HardwareCanvas once HardwareCanvas is more
            // stable. HardwareCanvas can be >10x faster than the software canvas rendering
            // for Android UI.
            Canvas surfaceCanvas = mSurface.lockCanvas(null);
            surfaceCanvas.drawColor(Color.TRANSPARENT, PorterDuff.Mode.CLEAR);
            drawSuper(surfaceCanvas);
            mSurface.unlockCanvasAndPost(surfaceCanvas);
        }
    }

    protected void drawSuper(Canvas canvas) {
        super.draw(canvas);
    }

    /* package */ View getInputTarget() {
        assert getChildCount() == 1;
        return getChildAt(0);
    }

    /* package */ void setSurface(Surface surface) {
        mSurface = surface;
        invalidate();
    }

    /* package */ void resize(int width, int height) {
        setLayoutParams(new FrameLayout.LayoutParams(width, height));
    }

    /* package */ void destroy() {
        getViewTreeObserver().removeOnPreDrawListener(mPredrawListener);
        mSurface = null;
    }
}
