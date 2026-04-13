// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.components;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;

/** Class for a CompositorButton that uses tint instead of multiple drawable resources. */
@NullMarked
public class TintedCompositorButton extends CompositorButton {
    // TODO(crbug.com/485925830): When we refactor to include some "LongPressHandler," infer this
    //  through the presence/absence of the handler.
    private final boolean mHasLongClickAction;

    private @ColorInt int mBackgroundTint;
    private @ColorInt int mTint;

    // Hover and pressed colors.
    private @ColorInt int mBackgroundHoverTint;
    private @ColorInt int mBackgroundTouchPressedTint;
    private @ColorInt int mBackgroundPeripheralPressedTint;

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

    /**
     * A set of Android colors to supply to the compositor.
     *
     * @param backgroundTint The background tint.
     * @param backgroundHoverTint The background hover tint.
     * @param backgroundTouchPressedTint The background touch pressed tint.
     * @param backgroundPeripheralPressedTint The background peripheral pressed tint.
     */
    public void setBackgroundTint(
            @ColorInt int backgroundTint,
            @ColorInt int backgroundHoverTint,
            @ColorInt int backgroundTouchPressedTint,
            @ColorInt int backgroundPeripheralPressedTint) {
        mBackgroundTint = backgroundTint;
        mBackgroundHoverTint = backgroundHoverTint;
        mBackgroundTouchPressedTint = backgroundTouchPressedTint;
        mBackgroundPeripheralPressedTint = backgroundPeripheralPressedTint;
    }

    /**
     * @return The button background tint (color value, NOT the resource Id) depending on the state
     *     of the button.
     */
    public @ColorInt int getBackgroundTint() {
        if (isHovered()) return mBackgroundHoverTint;
        if (isPressed()) {
            return isPressedFromMouse()
                    ? mBackgroundPeripheralPressedTint
                    : mBackgroundTouchPressedTint;
        }
        return mBackgroundTint;
    }
}
