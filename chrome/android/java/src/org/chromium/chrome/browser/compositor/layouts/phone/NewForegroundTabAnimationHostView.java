// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import android.content.Context;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.animation.RunOnNextLayout;
import org.chromium.ui.animation.RunOnNextLayoutDelegate;

/**
 * Host view for the new foreground tab animation.
 *
 * <p>This custom FrameLayout is designed to be a completely non-clipping container that covers the
 * entire parent view. The purpose is to force the whole view to be a "drawing" zone and avoid
 * {@link org.chromium.chrome.browser.hub.ShrinkExpandImageView} to be clipped when using view
 * properties like scale and translation. This problem is mostly present in multi-window mode.
 */
@NullMarked
public class NewForegroundTabAnimationHostView extends FrameLayout implements RunOnNextLayout {
    private final RunOnNextLayoutDelegate mRunOnNextLayoutDelegate;

    public NewForegroundTabAnimationHostView(Context context) {
        super(context);
        setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        setClipChildren(false);
        setClipToPadding(false);

        // Setting a background color forces the view to be considered a "drawing" view by Android.
        // This makes sure that the transformations (scale, translation) in ShrinkExpandImageView
        // are rendered correctly and not optimized away or improperly clipped.
        setBackgroundColor(ContextCompat.getColor(getContext(), android.R.color.transparent));

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
