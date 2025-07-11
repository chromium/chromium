// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player;

import static org.chromium.ui.base.ViewUtils.dpToPx;

import android.graphics.Rect;
import android.view.TouchDelegate;
import android.view.View;

import org.chromium.build.annotations.NullMarked;

/** Class for TouchDelegate helpers. */
@NullMarked
public class TouchDelegateUtil {
    /**
     * If a view's hit rect is smaller than 48x48dp in either dimension, set a TouchDelegate
     * centered on view that is at least 48x48.
     *
     * @param ancestor Ancestor of the view, which needs to forward touch events to the delegate.
     * @param view View on which to maybe set a TouchDelegate.
     * @return TouchDelegate.
     */
    public static TouchDelegate createTouchDelegate(View ancestor, View view) {
        final int minTargetSizePx = dpToPx(view.getContext(), 48);

        Rect target = new Rect();
        view.getHitRect(target);
        int viewWidth = target.width();
        int viewHeight = target.height();

        int newWidth = Math.max(viewWidth, minTargetSizePx);
        int newHeight = Math.max(viewHeight, minTargetSizePx);

        // Grow the hit rect around the center.
        int dx = ((newWidth / 2) - (viewWidth / 2));
        int dy = ((newHeight / 2) - (viewHeight / 2));
        target.left -= dx;
        target.top -= dy;
        target.right += dx;
        target.bottom += dy;
        TouchDelegate delegate = new TouchDelegate(target, view);
        ancestor.setTouchDelegate(delegate);
        return delegate;
    }

    private TouchDelegateUtil() {}
}
