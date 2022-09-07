// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Path;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.webkit.WebView;

/**
 * WebView subclass that can add Clip to canvas inside onDraw to trigger External Stencil behaviour.
 */
public class WebViewWithClipPath extends WebView {
    private Path mClipPath;
    private boolean mShouldClip;

    public WebViewWithClipPath(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    public void setEnableClipPath(boolean shouldClip) {
        this.mShouldClip = shouldClip;
        invalidate();
    }

    @Override
    protected void onSizeChanged(int newWidth, int newHeight, int oldWidth, int oldHeight) {
        int radius = 150;

        mClipPath = new Path();
        mClipPath.addRoundRect(
                new RectF(0, 0, newWidth, newHeight), radius, radius, Path.Direction.CW);

        super.onSizeChanged(newWidth, newHeight, oldWidth, oldHeight);
    }

    @Override
    protected void onDraw(Canvas canvas) {
        if (mShouldClip && mClipPath != null) {
            canvas.translate(getScrollX(), getScrollY());
            canvas.clipPath(mClipPath);
            canvas.translate(-getScrollX(), -getScrollY());
        }
        super.onDraw(canvas);
    }
}
