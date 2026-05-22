// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A reusable layout that fully encapsulates hover state tracking and bounds checking. */
@NullMarked
public class ImmersiveVideoHoverLayout extends LinearLayout {
    /** Listener for hover focus changes. */
    @FunctionalInterface
    public interface HoverListener {
        /** Called when the hover focus state changes. */
        void onHoverChanged(boolean hovered);
    }

    private @Nullable HoverListener mHoverListener;
    private boolean mIsCurrentlyHovered;

    public ImmersiveVideoHoverLayout(Context context) {
        super(context);
    }

    public ImmersiveVideoHoverLayout(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    public ImmersiveVideoHoverLayout(
            Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
    }

    public void setHoverListener(@Nullable HoverListener listener) {
        mHoverListener = listener;
    }

    private void handleHoverExit() {
        if (mIsCurrentlyHovered) {
            mIsCurrentlyHovered = false;
            if (mHoverListener != null) {
                mHoverListener.onHoverChanged(false);
            }
        }
    }

    @Override
    protected void onVisibilityChanged(View changedView, int visibility) {
        super.onVisibilityChanged(changedView, visibility);
        if (visibility != VISIBLE) {
            handleHoverExit();
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        handleHoverExit();
    }

    @Override
    public boolean dispatchHoverEvent(MotionEvent event) {
        int action = event.getActionMasked();

        if (mHoverListener != null) {
            switch (action) {
                case MotionEvent.ACTION_HOVER_ENTER:
                case MotionEvent.ACTION_HOVER_MOVE:
                    if (!mIsCurrentlyHovered) {
                        mIsCurrentlyHovered = true;
                        mHoverListener.onHoverChanged(true);
                    }
                    break;

                case MotionEvent.ACTION_HOVER_EXIT:
                    if (mIsCurrentlyHovered) {
                        mIsCurrentlyHovered = false;
                        mHoverListener.onHoverChanged(false);
                    }
                    break;
            }
        }

        return super.dispatchHoverEvent(event);
    }

    public @Nullable HoverListener getHoverListenerForTesting() {
        return mHoverListener;
    }
}
