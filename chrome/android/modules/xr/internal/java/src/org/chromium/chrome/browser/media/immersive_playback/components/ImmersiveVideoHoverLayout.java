// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
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

    /** Listener for accessibility focus changes. */
    @FunctionalInterface
    public interface AccessibilityFocusListener {
        /** Called when the accessibility focus state changes. */
        void onAccessibilityFocusChanged(boolean focused);
    }

    private @Nullable AccessibilityFocusListener mAccessibilityFocusListener;
    private boolean mIsAccessibilityFocused;

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

    public void setAccessibilityFocusListener(@Nullable AccessibilityFocusListener listener) {
        mAccessibilityFocusListener = listener;
    }

    private void handleHoverExit() {
        if (mIsCurrentlyHovered) {
            mIsCurrentlyHovered = false;
            if (mHoverListener != null) {
                mHoverListener.onHoverChanged(false);
            }
        }
    }

    private void handleAccessibilityFocusExit() {
        if (mIsAccessibilityFocused) {
            mIsAccessibilityFocused = false;
            if (mAccessibilityFocusListener != null) {
                mAccessibilityFocusListener.onAccessibilityFocusChanged(false);
            }
        }
    }

    @Override
    protected void onVisibilityChanged(View changedView, int visibility) {
        super.onVisibilityChanged(changedView, visibility);
        if (visibility != VISIBLE) {
            handleHoverExit();
            handleAccessibilityFocusExit();
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        handleHoverExit();
        handleAccessibilityFocusExit();
    }

    /**
     * Note: This method is only called for accessibility events dispatched by descendants. If this
     * layout itself becomes focusable and receives accessibility focus directly, those events will
     * not trigger this callback.
     */
    @Override
    public boolean requestSendAccessibilityEvent(View child, AccessibilityEvent event) {
        int eventType = event.getEventType();
        if (eventType == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED) {
            if (!mIsAccessibilityFocused) {
                mIsAccessibilityFocused = true;
                if (mAccessibilityFocusListener != null) {
                    mAccessibilityFocusListener.onAccessibilityFocusChanged(true);
                }
            }
        } else if (eventType == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED) {
            post(() -> checkAccessibilityFocus());
        }
        return super.requestSendAccessibilityEvent(child, event);
    }

    private void checkAccessibilityFocus() {
        boolean hasFocus = hasAccessibilityFocus();
        if (mIsAccessibilityFocused != hasFocus) {
            mIsAccessibilityFocused = hasFocus;
            if (mAccessibilityFocusListener != null) {
                mAccessibilityFocusListener.onAccessibilityFocusChanged(hasFocus);
            }
        }
    }

    private boolean hasAccessibilityFocus() {
        return isAccessibilityFocused() || hasAccessibilityFocus(this);
    }

    private boolean hasAccessibilityFocus(ViewGroup viewGroup) {
        for (int i = 0; i < viewGroup.getChildCount(); i++) {
            View child = viewGroup.getChildAt(i);
            if (child.isAccessibilityFocused()) {
                return true;
            }
            if (child instanceof ViewGroup && hasAccessibilityFocus((ViewGroup) child)) {
                return true;
            }
        }
        return false;
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
