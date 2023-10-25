// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.widget.ImageView;

/** {@link ImageView} for the Shrink, Expand, and New Tab animations. */
// TODO(crbug/1495731): Move to hub/internal/ once TabSwitcherLayout no longer depends on this.
public class ShrinkExpandImageView extends ImageView implements RunOnNextLayout {
    private final RunOnNextLayoutDelegate mRunOnNextLayoutDelegate;

    /**
     * @param context The Android {@link Context} for constructing the view.
     */
    public ShrinkExpandImageView(Context context) {
        super(context);
        mRunOnNextLayoutDelegate = new RunOnNextLayoutDelegate(this);
    }

    @Override
    public void onLayout(boolean changed, int l, int t, int r, int b) {
        super.onLayout(changed, l, t, r, b);
        runOnNextLayoutRunnables();
    }

    @Override
    public void runOnNextLayout(Runnable runnable) {
        mRunOnNextLayoutDelegate.runOnNextLayout(runnable);
    }

    @Override
    public void runOnNextLayoutRunnables() {
        mRunOnNextLayoutDelegate.runOnNextLayoutRunnables();
    }
}
