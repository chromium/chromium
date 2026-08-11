// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.components;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;

/** Class for a CompositorButton that uses tint instead of multiple drawable resources. */
@NullMarked
public class TintedCompositorButton extends CompositorButton {
    // TODO(crbug.com/485925830): When we refactor to include some "LongPressHandler," infer this
    //  through the presence/absence of the handler.
    private final boolean mHasLongClickAction;

    private boolean mIsHighlighted;

    private ColorStateList mBackgroundTint = ColorStateList.valueOf(Color.TRANSPARENT);
    private @ColorInt int mTint;

    public TintedCompositorButton(
            Context context,
            boolean incognito,
            @ButtonType int type,
            @Nullable StripLayoutView parentView,
            float width,
            float height,
            @Nullable TooltipHandler tooltipHandler,
            StripLayoutViewOnClickHandler clickHandler,
            StripLayoutViewOnKeyboardFocusHandler keyboardFocusHandler,
            @DrawableRes int resource,
            @DrawableRes int backgroundResource,
            float clickSlopDp) {
        this(
                context,
                incognito,
                type,
                parentView,
                width,
                height,
                tooltipHandler,
                clickHandler,
                keyboardFocusHandler,
                resource,
                backgroundResource,
                clickSlopDp,
                /* hasLongClickAction= */ false);
    }

    public TintedCompositorButton(
            Context context,
            boolean incognito,
            @ButtonType int type,
            @Nullable StripLayoutView parentView,
            float width,
            float height,
            @Nullable TooltipHandler tooltipHandler,
            StripLayoutViewOnClickHandler clickHandler,
            StripLayoutViewOnKeyboardFocusHandler keyboardFocusHandler,
            @DrawableRes int resource,
            @DrawableRes int backgroundResource,
            float clickSlopDp,
            boolean hasLongClickAction) {
        super(
                context,
                incognito,
                resource,
                backgroundResource,
                type,
                parentView,
                width,
                height,
                tooltipHandler,
                clickHandler,
                keyboardFocusHandler,
                clickSlopDp);
        mHasLongClickAction = hasLongClickAction;
    }

    @Override
    public boolean hasLongClickAction() {
        return mHasLongClickAction;
    }

    /**
     * Sets whether the button has an active/highlighted state independent of touch gestures.
     *
     * @param isHighlighted The highlight state.
     */
    public void setHighlighted(boolean isHighlighted) {
        mIsHighlighted = isHighlighted;
    }

    /**
     * Returns whether the button is in an active/highlighted state independent of touch gestures.
     */
    public boolean isHighlighted() {
        return mIsHighlighted;
    }

    /**
     * @param tint The tint.
     */
    public void setTint(@ColorInt int tint) {
        mTint = tint;
    }

    /**
     * @return The icon tint (color value, NOT the resource Id) depending on the state of the
     *     button.
     */
    public @ColorInt int getTint() {
        return mTint;
    }

    /** Sets the button's background tint to the given ColorStateList. */
    public void setBackgroundTint(ColorStateList backgroundTint) {
        mBackgroundTint = backgroundTint;
    }

    /**
     * @return The button background tint (color value, NOT the resource Id) depending on the state
     *     of the button.
     */
    public @ColorInt int getBackgroundTint() {
        return resolveColor(mBackgroundTint);
    }

    private @ColorInt int resolveColor(ColorStateList csl) {
        int stateCount = 0;
        if (isHovered()) stateCount++;
        if (isPressed()) stateCount++;
        if (isPressedFromMouse()) stateCount++;
        if (isHighlighted()) stateCount++;

        int[] stateSet = new int[stateCount];
        int i = 0;
        if (isHovered()) stateSet[i++] = android.R.attr.state_hovered;
        if (isPressed()) stateSet[i++] = android.R.attr.state_pressed;
        if (isPressedFromMouse()) stateSet[i++] = R.attr.state_peripheral_pressed;
        if (isHighlighted()) stateSet[i++] = android.R.attr.state_activated;

        return csl.getColorForState(stateSet, csl.getDefaultColor());
    }
}
