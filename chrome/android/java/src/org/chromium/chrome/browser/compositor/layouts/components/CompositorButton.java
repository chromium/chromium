// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.components;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.RectF;
import android.util.FloatProperty;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;
import org.chromium.ui.MotionEventUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * {@link CompositorButton} keeps track of state for buttons that are rendered in the compositor.
 */
public class CompositorButton extends StripLayoutView {
    /**
     * A property that can be used with a {@link
     * org.chromium.chrome.browser.layouts.animation.CompositorAnimator}.
     */
    public static final FloatProperty<CompositorButton> OPACITY =
            new FloatProperty<CompositorButton>("opacity") {
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
    // @StripLayoutView the button was embedded in. Null if it's not a child view.
    @Nullable private final StripLayoutView mParentView;
    private final @ButtonType int mType;

    /**
     * Default constructor for {@link CompositorButton}
     *
     * @param context An Android context for fetching dimens.
     * @param width The button width.
     * @param height The button height.
     * @param clickHandler The action to be performed on click.
     */
    public CompositorButton(
            Context context,
            @ButtonType int type,
            StripLayoutView parentView,
            float width,
            float height,
            StripLayoutViewOnClickHandler clickHandler) {
        super(false, clickHandler);
        mDrawBounds.set(0, 0, width, height);

        mType = type;
        mOpacity = 1.f;
        mIsPressed = false;
        mParentView = parentView;
        setVisible(true);

        Resources res = context.getResources();
        float sPxToDp = 1.0f / res.getDisplayMetrics().density;
        float clickSlop = res.getDimension(R.dimen.compositor_button_slop) * sPxToDp;
        setTouchTargetInsets(-clickSlop, -clickSlop, -clickSlop, -clickSlop);
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
     * @param fromMouse Whether the event originates from a mouse.
     */
    public void setPressed(boolean state, boolean fromMouse) {
        mIsPressed = state;
        mIsPressedFromMouse = fromMouse;
    }

    /**
     * @param slop The additional area outside of the button to be considered when checking click
     *     target bounds.
     */
    public void setClickSlop(float slop) {
        setTouchTargetInsets(-slop, -slop, -slop, -slop);
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
     * @param fromMouse Whether the event originates from a mouse.
     * @param buttons State of all buttons that were pressed when onDown was invoked.
     * @return Whether or not the button was hit.
     */
    public boolean onDown(float x, float y, boolean fromMouse, int buttons) {
        if (checkClickedOrHovered(x, y)
                && MotionEventUtils.isTouchOrPrimaryButton(fromMouse, buttons)) {
            setPressed(true, fromMouse);
            return true;
        }
        return false;
    }

    /**
     * @param x The x offset of the event.
     * @param y The y offset of the event.
     * @param fromMouse Whether the event originates from a mouse.
     * @param buttons State of all buttons that were pressed when onDown was invoked.
     * @return Whether or not the button was clicked.
     */
    public boolean click(float x, float y, boolean fromMouse, int buttons) {
        if (checkClickedOrHovered(x, y)
                && MotionEventUtils.isTouchOrPrimaryButton(fromMouse, buttons)) {
            setPressed(false, false);
            return true;
        }
        return false;
    }

    /**
     * Set state for an onUpOrCancel event.
     * @return Whether or not the button was selected.
     */
    public boolean onUpOrCancel() {
        boolean state = isPressed();
        setPressed(false, false);
        return state;
    }

    /**
     * Set whether button is hovered on.
     *
     * @param isHovered Whether the button is hovered on.
     */
    public void setHovered(boolean isHovered) {
        mIsHovered = isHovered;
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
