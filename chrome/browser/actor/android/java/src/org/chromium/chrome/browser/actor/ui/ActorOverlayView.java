// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor.ui;

import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.StateListDrawable;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.PointerIcon;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.FrameLayout;

import androidx.core.content.ContextCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.util.MotionEventUtils;

/**
 * The root view for the Actor Overlay. Displays the overlay content on top of the browser content.
 */
@NullMarked
public class ActorOverlayView extends FrameLayout {
    private static final int[] STATE_PRESSED = new int[] {android.R.attr.state_pressed};
    private static final int[] STATE_HOVERED = new int[] {android.R.attr.state_hovered};

    private boolean mWasChildHovered;

    public ActorOverlayView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setClickable(true);
        setFocusable(true);
        setPointerIcon(PointerIcon.getSystemIcon(context, PointerIcon.TYPE_NO_DROP));

        InnerGlowDrawable normalDrawable = InnerGlowDrawable.createMainWebpageGlow(context);
        InnerGlowDrawable normalDrawableForHover = InnerGlowDrawable.createMainWebpageGlow(context);

        int hoverColor = ContextCompat.getColor(context, R.color.actor_overlay_hover_color);
        Drawable hoverHighlight = new ColorDrawable(hoverColor);
        Drawable hoverDrawable =
                new LayerDrawable(new Drawable[] {hoverHighlight, normalDrawableForHover});

        StateListDrawable stateListDrawable = new StateListDrawable();
        stateListDrawable.addState(STATE_PRESSED, hoverDrawable);
        stateListDrawable.addState(STATE_HOVERED, hoverDrawable);
        stateListDrawable.addState(new int[] {}, normalDrawable);

        setBackground(stateListDrawable);
    }

    /**
     * Sets the margins of the view.
     *
     * @param left The left margin in pixels.
     * @param top The top margin in pixels.
     * @param right The right margin in pixels.
     * @param bottom The bottom margin in pixels.
     */
    public void setMargins(int left, int top, int right, int bottom) {
        MarginLayoutParams params = (MarginLayoutParams) getLayoutParams();
        if (params == null) return;

        if (params.leftMargin != left
                || params.topMargin != top
                || params.rightMargin != right
                || params.bottomMargin != bottom) {
            params.leftMargin = left;
            params.topMargin = top;
            params.rightMargin = right;
            params.bottomMargin = bottom;
            setLayoutParams(params);
        }
    }

    private boolean isPointInView(float x, float y) {
        return x >= 0 && x < getWidth() && y >= 0 && y < getHeight();
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        boolean handled = super.dispatchTouchEvent(event);
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
            // When a click starts inside the view, we intercept the native ACTION_HOVER_EXIT
            // event to keep the hover scrim active (dark). However, if the user drags their
            // cursor outside the view bounds and releases the click (ACTION_UP or ACTION_CANCEL
            // outside bounds), the hover state would remain stuck at 'true' forever since no
            // subsequent hover exit event will be delivered by the OS.
            // We manually clear the hover state when the touch gesture ends outside view bounds.
            if (!isPointInView(event.getX(), event.getY())) {
                setHovered(false);
            }
        }
        return handled;
    }

    @Override
    public boolean dispatchHoverEvent(MotionEvent event) {
        // When a user clicks/taps using a trackpad or mouse, the Android framework sends an
        // ACTION_HOVER_EXIT event *before* sending the ACTION_DOWN event. By default, this exit
        // event clears the hover state, causing the background scrim to momentarily flicker
        // back to normal (light) mode.
        // We intercept and swallow ACTION_HOVER_EXIT events if the pointer is still inside the
        // view bounds (e.g. click happened on the scrim), preventing the flicker. We only let
        // the exit event propagate if the pointer actually leaves the view bounds.
        if (event.getActionMasked() == MotionEvent.ACTION_HOVER_EXIT
                && isPointInView(event.getX(), event.getY())) {
            return true;
        }
        boolean handled = super.dispatchHoverEvent(event);
        boolean childHovered =
                handled && !isHovered() && event.getActionMasked() != MotionEvent.ACTION_HOVER_EXIT;
        if (mWasChildHovered != childHovered) {
            mWasChildHovered = childHovered;
            refreshDrawableState();
        }
        return true;
    }

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (MotionEventUtils.isPointerEvent(event)) {
            return true;
        }
        return super.onGenericMotionEvent(event);
    }

    // Overridden to preserve the overlay's hover state when a child view receives hover focus.
    // When a child is hovered, dispatchHoverEvent clears isHovered() on the parent. We must
    // explicitly merge STATE_HOVERED to keep the overlay scrim hovered.
    @Override
    protected int[] onCreateDrawableState(int extraSpace) {
        if (!isHovered() && mWasChildHovered) {
            int[] drawableState = super.onCreateDrawableState(extraSpace + 1);
            mergeDrawableStates(drawableState, STATE_HOVERED);
            return drawableState;
        }
        return super.onCreateDrawableState(extraSpace);
    }
}
