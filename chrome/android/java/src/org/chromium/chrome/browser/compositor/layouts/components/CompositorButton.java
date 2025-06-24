// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.components;

import android.content.Context;
import android.graphics.RectF;
import android.util.FloatProperty;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.ui.util.MotionEventUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * {@link CompositorButton} keeps track of state for buttons that are rendered in the compositor.
 */
@NullMarked
public class CompositorButton extends StripLayoutView {
    /**
     * A property that can be used with a {@link
     * org.chromium.chrome.browser.layouts.animation.CompositorAnimator}.
     */
    public static final FloatProperty<CompositorButton> OPACITY =
            new FloatProperty<>("opacity") {
                @Override
                public void setValue(CompositorButton object, float value) {
                    object.setOpacity(value);
                }

                @Override
                public Float get(CompositorButton object) {
                    return object.getOpacity();
                }
            };

    @IntDef({ButtonType.NEW_TAB, ButtonType.INCOGNITO_SWITCHER, ButtonType.TAB_CLOSE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ButtonType {
        int NEW_TAB = 0;
        int INCOGNITO_SWITCHER = 1;
        int TAB_CLOSE = 2;
    }

    public interface TooltipHandler {
        void setTooltipText(String text);
    }

    protected int mResource;
    protected int mBackgroundResource;

    private int mPressedResource;
    private int mIncognitoResource;
    private int mIncognitoPressedResource;

    private float mOpacity;
    private boolean mIsPressed;
    private boolean mIsPressedFromMouse;
    private boolean mIsHovered;
    private String mAccessibilityDescriptionIncognito = "";

    private final @Nullable TooltipHandler mTooltipHandler;

    // @StripLayoutView the button was embedded in. Null if it's not a child view.
    @Nullable private final StripLayoutView mParentView;
    private final @ButtonType int mType;
    private final float mClickSlop;

    /**
     * Default constructor for {@link CompositorButton}
     *
     * @param context An Android context for fetching dimens.
     * @param width The button width.
     * @param height The button height.
     * @param clickHandler The action to be performed on click.
     * @param keyboardFocusHandler The action to be performed on keyboard focus.
     * @param clickSlopDp The click slop for the button, in dp.
     */
    public CompositorButton(
            Context context,
            @ButtonType int type,
            @Nullable StripLayoutView parentView,
            float width,
            float height,
            @Nullable TooltipHandler tooltipHandler,
            StripLayoutViewOnClickHandler clickHandler,
            StripLayoutViewOnKeyboardFocusHandler keyboardFocusHandler,
            float clickSlopDp) {
        super(false, clickHandler, keyboardFocusHandler, context);
        mDrawBounds.set(0, 0, width, height);

        mType = type;
        mOpacity = 1.f;
        mIsPressed = false;
        mParentView = parentView;
        mTooltipHandler = tooltipHandler;
        setVisible(true);

        mClickSlop = clickSlopDp;
        // Apply the click slop to the button's touch target.
        setTouchTargetInsets(null, null, null, null);
    }

    /**
     * A set of Android resources to supply to the compositor.
     * @param resource                  The default Android resource.
     * @param pressedResource           The pressed Android resource.
     * @param incognitoResource         The incognito Android resource.
     * @param incognitoPressedResource  The incognito pressed resource.
     */
    public void setResources(
            int resource,
            int pressedResource,
            int incognitoResource,
            int incognitoPressedResource) {
        mResource = resource;
        mPressedResource = pressedResource;
        mIncognitoResource = incognitoResource;
        mIncognitoPressedResource = incognitoPressedResource;
    }

    /**
     * @param description A string describing the resource.
     */
    public void setAccessibilityDescription(String description, String incognitoDescription) {
        super.setAccessibilityDescription(description);
        mAccessibilityDescriptionIncognito = incognitoDescription;
    }

    /** {@link org.chromium.chrome.browser.layouts.components.VirtualView} Implementation */
    @Override
    public String getAccessibilityDescription() {
        return isIncognito()
                ? mAccessibilityDescriptionIncognito
                : super.getAccessibilityDescription();
    }

    @Override
    public boolean checkClickedOrHovered(float x, float y) {
        if (mOpacity < 1.f || !isVisible()) return false;
        return super.checkClickedOrHovered(x, y);
    }

    /**
     * @return Parent view this button is embedded in.
     */
    public @Nullable StripLayoutView getParentView() {
        return mParentView;
    }

    /**
     * @param bounds A {@link RectF} representing the location of the button.
     */
    public void setBounds(RectF bounds) {
        mDrawBounds.set(bounds);
    }

    /**
     * @return The opacity of the button.
     */
    public float getOpacity() {
        return mOpacity;
    }

    /**
     * @param opacity The opacity of the button.
     */
    public void setOpacity(float opacity) {
        mOpacity = opacity;
    }

    /**
     * @return Type for this button.
     */
    public @ButtonType int getType() {
        return mType;
    }

    /**
     * @return The pressed state of the button.
     */
    public boolean isPressed() {
        return mIsPressed;
    }

    /**
     * @param state The pressed state of the button.
     */
    public void setPressed(boolean state) {
        mIsPressed = state;

        // clear isPressedFromMouse state.
        if (!state) {
            setPressedFromMouse(false);
        }
    }

    /**
     * @param state The pressed state of the button.
     * @param fromMousePrimaryButton Whether the event originates from a mouse.
     */
    public void setPressed(boolean state, boolean fromMousePrimaryButton) {
        mIsPressed = state;
        mIsPressedFromMouse = fromMousePrimaryButton;
    }

    /**
     * Do not account for the button's click slop in the method inputs when invoking this method as
     * this is accounted for in this method.
     */
    @Override
    public void setTouchTargetInsets(
            @Nullable Float left,
            @Nullable Float top,
            @Nullable Float right,
            @Nullable Float bottom) {
        float leftInset = -mClickSlop + (left != null ? left : 0);
        float topInset = -mClickSlop + (top != null ? top : 0);
        float rightInset = -mClickSlop + (right != null ? right : 0);
        float bottomInset = -mClickSlop + (bottom != null ? bottom : 0);
        super.setTouchTargetInsets(leftInset, topInset, rightInset, bottomInset);
    }

    /**
     * @return The Android resource id for this button based on it's state.
     */
    public int getResourceId() {
        if (isPressed()) {
            return isIncognito() ? mIncognitoPressedResource : mPressedResource;
        }
        return isIncognito() ? mIncognitoResource : mResource;
    }

    /**
     * Set state for a drag event.
     * @param x     The x offset of the event.
     * @param y     The y offset of the event.
     * @return      Whether or not the button is selected after the event.
     */
    public boolean drag(float x, float y) {
        if (!checkClickedOrHovered(x, y)) {
            setPressed(false);
            return false;
        }
        return isPressed();
    }

    /**
     * Set state for an onDown event.
     *
     * @param x The x offset of the event.
     * @param y The y offset of the event.
     * @param buttons State of all buttons that were pressed when onDown was invoked.
     * @return Whether or not the button was hit.
     */
    public boolean onDown(float x, float y, int buttons) {
        if (checkClickedOrHovered(x, y) && MotionEventUtils.isTouchOrPrimaryButton(buttons)) {
            setPressed(true, MotionEventUtils.isPrimaryButton(buttons));
            return true;
        }
        return false;
    }

    /**
     * @param x The x offset of the event.
     * @param y The y offset of the event.
     * @param buttons State of all buttons that were pressed when onDown was invoked.
     * @return Whether or not the button was clicked.
     */
    public boolean click(float x, float y, int buttons) {
        if (checkClickedOrHovered(x, y) && MotionEventUtils.isTouchOrPrimaryButton(buttons)) {
            setPressed(false, MotionEventUtils.isPrimaryButton(buttons));
            return true;
        }
        return false;
    }

    /**
     * Set state for an onUpOrCancel event.
     *
     * @return Whether or not the button was selected.
     */
    public boolean onUpOrCancel() {
        boolean state = isPressed();
        setPressed(/* state= */ false, /* fromMousePrimaryButton= */ false);
        return state;
    }

    /**
     * Set whether button is hovered on and notify the tooltip handler if the hover state changed.
     *
     * @param isHovered Whether the button is hovered on.
     */
    public void setHovered(boolean isHovered) {
        if (mTooltipHandler != null && mIsHovered != isHovered) {
            mTooltipHandler.setTooltipText(isHovered ? getAccessibilityDescription() : "");
        }
        mIsHovered = isHovered;
    }

    @Override
    public void setVisible(boolean isVisible) {
        if (!isVisible) {
            setHovered(false);
        }
        super.setVisible(isVisible);
    }

    /**
     * @return Whether the button is hovered on.
     */
    public boolean isHovered() {
        return mIsHovered;
    }

    /**
     * Set whether the button is pressed from mouse.
     *
     * @param isPressedFromMouse Whether the button is pressed from mouse.
     */
    private void setPressedFromMouse(boolean isPressedFromMouse) {
        mIsPressedFromMouse = isPressedFromMouse;
    }

    /**
     * @return Whether the button is pressed from mouse.
     */
    public boolean isPressedFromMouse() {
        return mIsPressed && mIsPressedFromMouse;
    }

    /**
     * @return Whether hover background should be applied to the button.
     */
    public boolean getShouldApplyHoverBackground() {
        return isHovered() || isPressedFromMouse();
    }
}
