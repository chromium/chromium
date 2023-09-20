// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.widget.ImageView;

/**
 * Custom image view representing a tab thumbnail that invokes a {@link Runnable} in response to
 * a layout happening. Used for {@link TabSwitcherLayout} animations.
 */
class TabImageView extends ImageView {
    private Runnable mOnNextImageViewLayout;

    TabImageView(Context context) {
        super(context);
    }

    @Override
    public void onLayout(boolean changed, int l, int t, int r, int b) {
        super.onLayout(changed, l, t, r, b);
        runOnNextLayoutRunnable();
    }

    /**
     * Sets a runnable to run on the next layout or immediately if there is no layout planned.
     * @param runnable The {@link Runnable} to invoke now or on the next layout.
     */
    void setOnNextLayoutRunnable(Runnable runnable) {
        assert mOnNextImageViewLayout == null : "OnNextLayoutRunnable already set.";

        mOnNextImageViewLayout = runnable;
        if (!isAttachedToWindow() || !isLayoutRequested()) {
            runOnNextLayoutRunnable();
        }
    }

    /**
     * Runs the on next layout runnable. Not private so that the {@link TabSwitcherLayout} can
     * invoke this when forcing animations to complete.
     */
    void runOnNextLayoutRunnable() {
        if (mOnNextImageViewLayout != null) {
            Runnable r = mOnNextImageViewLayout;
            mOnNextImageViewLayout = null;
            r.run();
        }
    }
}
