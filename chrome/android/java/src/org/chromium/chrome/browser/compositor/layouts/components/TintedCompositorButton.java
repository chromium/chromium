// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.components;

import android.content.Context;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutView;

/** Class for a CompositorButton that uses tint instead of multiple drawable resources. */
public class TintedCompositorButton extends CompositorButton {
    private Context mContext;

    private @ColorInt int mBackgroundDefaultTint;
    private @ColorInt int mBackgroundPressedTint;
    private @ColorInt int mBackgroundIncognitoTint;
    private @ColorInt int mBackgroundIncognitoPressedTint;
    private @ColorInt int mDefaultTint;
    private @ColorInt int mPressedTint;
    private @ColorInt int mIncognitoTint;
    private @ColorInt int mIncognitoPressedTint;

    // Hover and pressed colors for Advanced peripheral support(APS).
    private @ColorInt int mApsHoverBackgroundDefaultTint;
    private @ColorInt int mApsBackgroundPressedTint;
    private @ColorInt int mApsHoverBackgroundIncognitoTint;
    private @ColorInt int mApsBackgroundIncognitoPressedTint;

    public TintedCompositorButton(
            Context context,
            @ButtonType int type,
            StripLayoutView parentView,
            float width,
            float height,
            StripLayoutViewOnClickHandler clickHandler,
            @DrawableRes int resource) {
        super(context, type, parentView, width, height, clickHandler);
        mContext = context;
        mResource = resource;
    }

    /*
     * This method should not be called. Use setResource and setTintResources instead.
     */
    @Override
    public void setResources(
            int resource,
            int pressedResource,
            int incognitoResource,
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
     * @param backgroundResource The default Android resource.
     */
    public void setBackgroundResourceId(@DrawableRes int backgroundResource) {
        mBackgroundResource = backgroundResource;
    }

    /**
     * @return The Android resource that represents button background.
     */
    public int getBackgroundResourceId() {
        return mBackgroundResource;
    }

    /**
     * A set of Android resources to supply to the compositor.
     * @param defaultTint           The default tint resource.
     * @param pressedTint           The pressed tint resource.
     * @param incognitoTint         The incognito tint resource.
     * @param incognitoPressedTint  The incognito pressed tint resource.
     */
    public void setTintResources(
            @ColorRes int defaultTint,
            @ColorRes int pressedTint,
            @ColorRes int incognitoTint,
            @ColorRes int incognitoPressedTint) {
        setTint(
                AppCompatResources.getColorStateList(mContext, defaultTint).getDefaultColor(),
                AppCompatResources.getColorStateList(mContext, pressedTint).getDefaultColor(),
                AppCompatResources.getColorStateList(mContext, incognitoTint).getDefaultColor(),
                AppCompatResources.getColorStateList(mContext, incognitoPressedTint)
                        .getDefaultColor());
    }

    /**
     * @param defaultTint The default tint.
     * @param pressedTint The pressed tint.
     * @param incognitoTint The incognito tint.
     * @param incognitoPressedTint The incognito pressed tint.
     */
    public void setTint(
            @ColorInt int defaultTint,
            @ColorInt int pressedTint,
            @ColorInt int incognitoTint,
            @ColorInt int incognitoPressedTint) {
        mDefaultTint = defaultTint;
        mPressedTint = pressedTint;
        mIncognitoTint = incognitoTint;
        mIncognitoPressedTint = incognitoPressedTint;
    }

    /**
     * A set of Android colors to supply to the compositor.
     *
     * @param backgroundDefaultTint The default background tint.
     * @param backgroundPressedTint The pressed background tint.
     * @param backgroundIncognitoTint The incognito background tint.
     * @param backgroundIncognitoPressedTint The incognito pressed background tint.
     * @param apsHoverBackgroundDefaultTint The aps hover background tint.
     * @param apsBackgroundPressedTint The aps pressed background tint.
     * @param apsHoverBackgroundIncognitoTint The aps incognito hover background tint.
     * @param apsBackgroundIncognitoPressedTint The aps pressed incognito background tint.
     */
    public void setBackgroundTint(
            @ColorInt int backgroundDefaultTint,
            @ColorInt int backgroundPressedTint,
            @ColorInt int backgroundIncognitoTint,
            @ColorInt int backgroundIncognitoPressedTint,
            @ColorInt int apsHoverBackgroundDefaultTint,
            @ColorInt int apsBackgroundPressedTint,
            @ColorInt int apsHoverBackgroundIncognitoTint,
            @ColorInt int apsBackgroundIncognitoPressedTint) {
        mBackgroundDefaultTint = backgroundDefaultTint;
        mBackgroundPressedTint = backgroundPressedTint;
        mBackgroundIncognitoTint = backgroundIncognitoTint;
        mBackgroundIncognitoPressedTint = backgroundIncognitoPressedTint;
        mApsHoverBackgroundDefaultTint = apsHoverBackgroundDefaultTint;
        mApsBackgroundPressedTint = apsBackgroundPressedTint;
        mApsHoverBackgroundIncognitoTint = apsHoverBackgroundIncognitoTint;
        mApsBackgroundIncognitoPressedTint = apsBackgroundIncognitoPressedTint;
    }

    /**
     * @return The icon tint (color value, NOT the resource Id) depending on the state of the button
     *         and the tab (incognito or not).
     */
    public @ColorInt int getTint() {
        int tint = isIncognito() ? mIncognitoTint : mDefaultTint;
        if (isPressed()) {
            tint = isIncognito() ? mIncognitoPressedTint : mPressedTint;
        }
        return tint;
    }

    /**
     * @return The button background tint (color value, NOT the resource Id) depending on the state
     *         of the button and the tab.
     */
    public @ColorInt int getBackgroundTint() {
        int tint = isIncognito() ? mBackgroundIncognitoTint : mBackgroundDefaultTint;
        if (isHovered()) {
            tint =
                    isIncognito()
                            ? mApsHoverBackgroundIncognitoTint
                            : mApsHoverBackgroundDefaultTint;
        } else {
            if (isPressed()) {
                if (isPressedFromMouse()) {
                    tint =
                            isIncognito()
                                    ? mApsBackgroundIncognitoPressedTint
                                    : mApsBackgroundPressedTint;
                } else {
                    tint = isIncognito() ? mBackgroundIncognitoPressedTint : mBackgroundPressedTint;
                }
            }
        }
        return tint;
    }
}
