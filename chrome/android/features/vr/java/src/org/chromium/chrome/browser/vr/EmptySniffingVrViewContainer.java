// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.widget.FrameLayout;

/**
 * A {@link VrViewContainer} that is able to detect whether or not anything has been drawn inside
 * of it.
 *
 * TODO(mthiesse): See if we can walk the view hierarchy to detect that no elements are visible
 * rather than extending the canvas to detect whether draw calls were issued, and if that's actually
 * faster.
 */
public class EmptySniffingVrViewContainer extends VrViewContainer {
    private NoopCanvas mNoopCanvas;
    private Boolean mEmpty;
    private EmptyListener mListener;

    /**
     * Informs the listener when the VrViewContainer is and is not empty.
     */
    public static interface EmptyListener {
        public void onVrViewEmpty();
        public void onVrViewNonEmpty();
    }

    /**
     * See {@link FrameLayout#FrameLayout(Context)}.
     */
    public EmptySniffingVrViewContainer(Context context, EmptyListener listener) {
        super(context);
        mListener = listener;
    }

    @Override
    /* package */ void resize(int width, int height) {
        mNoopCanvas = new NoopCanvas(Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888),
                true /* preGenerateException */);
        super.resize(width, height);
        // Ensure we draw after creating the noop canvas.
        postInvalidate();
    }

    @Override
    protected void drawSuper(Canvas canvas) {
        try {
            if (mNoopCanvas != null) super.drawSuper(mNoopCanvas);
            onEmpty();
        } catch (NoopCanvas.NoopException exception) {
            mNoopCanvas.restoreToCount(1);
            super.drawSuper(canvas);
            onNonEmpty();
        }
    }

    private void onEmpty() {
        if (mEmpty != null && mEmpty) return;
        mEmpty = true;
        mListener.onVrViewEmpty();
    }

    private void onNonEmpty() {
        if (mEmpty != null && !mEmpty) return;
        mEmpty = false;
        mListener.onVrViewNonEmpty();
    }
}
