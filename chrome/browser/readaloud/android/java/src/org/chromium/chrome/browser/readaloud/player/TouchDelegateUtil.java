// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud.player;

import android.graphics.Rect;
import android.view.TouchDelegate;
import android.view.View;

/** Class for TouchDelegate helpers. */
public class TouchDelegateUtil {
    /**
     * Set a TouchDelegate on view's parent so that view's touch target is doubled in width and
     * height.
     *
     * @param view View whose touch target needs to be bigger.
     */
    public static void setBiggerTouchTarget(View view) {
        Rect target = new Rect();
        view.getHitRect(target);
        int halfWidth = target.width() / 2;
        int halfHeight = target.height() / 2;
        target.left -= halfWidth;
        target.top -= halfHeight;
        target.right += halfWidth;
        target.bottom += halfHeight;
        ((View) view.getParent()).setTouchDelegate(new TouchDelegate(target, view));
    }

    private TouchDelegateUtil() {}
}
