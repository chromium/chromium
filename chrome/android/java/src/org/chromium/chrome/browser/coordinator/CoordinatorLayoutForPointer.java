// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.coordinator;

import android.content.Context;
import android.os.Build;
import android.support.design.widget.CoordinatorLayout;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.View;

/**
 * This class overrides {@link onResolvePointerIcon} method to correctly determine the pointer icon
 * from a mouse motion event. This is needed because the default android impl does not consider
 * view visibility.
 */
public class CoordinatorLayoutForPointer extends CoordinatorLayout {
    public CoordinatorLayoutForPointer(Context context, AttributeSet attrs) {
        super(context, attrs);
        setFocusable(false);
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
    }

    private boolean isWithinBoundOfView(int x, int y, View view) {
        return ((x >= view.getLeft() && x <= view.getRight())
                && (y >= view.getTop() && y <= view.getBottom()));
    }

    @Override
    public PointerIcon onResolvePointerIcon(MotionEvent event, int pointerIndex) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) return null;

        final int x = (int) event.getX(pointerIndex);
        final int y = (int) event.getY(pointerIndex);
        final int childrenCount = getChildCount();
        for (int i = childrenCount - 1; i >= 0; --i) {
            if (getChildAt(i).getVisibility() != VISIBLE) continue;
            if (isWithinBoundOfView(x, y, getChildAt(i))) {
                return getChildAt(i).onResolvePointerIcon(event, pointerIndex);
            }
        }
        return super.onResolvePointerIcon(event, pointerIndex);
    }
}
