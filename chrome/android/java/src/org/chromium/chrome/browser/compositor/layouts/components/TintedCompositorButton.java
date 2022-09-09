// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.components;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;

/**
 * Class for a CompositorButton that uses tint instead of multiple drawable resources.
 */
public class TintedCompositorButton extends CompositorButton {
    private Context mContext;

    private @ColorRes int mDefaultTintResource;
    private @ColorRes int mPressedTintResource;
    private @ColorRes int mIncognitoTintResource;
    private @ColorRes int mIncognitoPressedTintResource;

    public TintedCompositorButton(
            Context context, float width, float height, CompositorOnClickHandler clickHandler) {
        super(context, width, height, clickHandler);

        mContext = context;
    }

    public TintedCompositorButton(Context context, float width, float height,
            CompositorOnClickHandler clickHandler, @DrawableRes int resource) {
        super(context, width, height, clickHandler);

        mContext = context;
        mResource = resource;
    }

    /*
     * This method should not be called. Use setResource and setTintResources instead.
     */
    @Override
    public void setResources(int resource, int pressedResource, int incognitoResource,
            int incognitoPressedResource) {
        throw new UnsupportedOperationException();
    }

    /**
     * @param resource The default Android resource.
     */
    public void setResource(@DrawableRes int resource) {
        mResource = resource;
    }

    /**
     * @return The default Android resource.
     */
    @Override
    public int getResourceId() {
        return mResource;
    }

    /**
     * A set of Android resources to supply to the compositor.
     * @param defaultTint           The default tint resource.
     * @param pressedTint           The pressed tint resource.
     * @param incognitoTint         The incognito tint resource.
     * @param incognitoPressedTint  The incognito pressed tint resource.
     */
    public void setTintResources(@ColorRes int defaultTint, @ColorRes int pressedTint,
            @ColorRes int incognitoTint, @ColorRes int incognitoPressedTint) {
        mDefaultTintResource = defaultTint;
        mPressedTintResource = pressedTint;
        mIncognitoTintResource = incognitoTint;
        mIncognitoPressedTintResource = incognitoPressedTint;
    }

    /**
     * @return The tint (color value, NOT the resource Id) depending on the state of the button and
     *         the tab (incognito or not).
     */
    public @ColorInt int getTint() {
        int tint = isIncognito() ? mIncognitoTintResource : mDefaultTintResource;
        if (isPressed()) {
            tint = isIncognito() ? mIncognitoPressedTintResource : mPressedTintResource;
        }

        return AppCompatResources.getColorStateList(mContext, tint).getDefaultColor();
    }
}
