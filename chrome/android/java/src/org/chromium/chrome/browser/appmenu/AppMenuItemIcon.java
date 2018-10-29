// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.appmenu;

import android.content.Context;
import android.support.v7.widget.AppCompatImageView;
import android.util.AttributeSet;
import android.widget.Checkable;

/**
 * An AppCompatImageView that supports the checkable state.
 */
public class AppMenuItemIcon extends AppCompatImageView implements Checkable {
    private static final int[] CHECKED_STATE_SET = new int[] {android.R.attr.state_checked};
    private boolean mCheckedState;

    public AppMenuItemIcon(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void setChecked(boolean state) {
        if (state == mCheckedState) return;
        mCheckedState = state;
        refreshDrawableState();
    }

    @Override
    public int[] onCreateDrawableState(int extraSpace) {
        final int[] drawableState = super.onCreateDrawableState(extraSpace + 1);
        if (mCheckedState) {
            mergeDrawableStates(drawableState, CHECKED_STATE_SET);
        }
        return drawableState;
    }

    @Override
    public boolean isChecked() {
        return mCheckedState;
    }

    @Override
    public void toggle() {
        setChecked(!mCheckedState);
    }
}