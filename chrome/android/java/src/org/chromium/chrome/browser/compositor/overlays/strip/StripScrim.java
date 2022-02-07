// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.compositor.overlays.strip;

import org.chromium.chrome.R;
/**
 * Tab strip scrim properties.
 */
public class StripScrim {
    private float mDrawX;
    private float mWidth;
    private float mHeight;
    private float mAlpha;
    private boolean mIsShowing;

    public StripScrim(float width, float height) {
        this.mWidth = width;
        this.mHeight = height;
    }

    public float getX() {
        return mDrawX;
    }

    public void setX(float mDrawX) {
        this.mDrawX = mDrawX;
    }

    public float getWidth() {
        return mWidth;
    }

    public float getHeight() {
        return mHeight;
    }

    public void setWidth(float mWidth) {
        this.mWidth = mWidth;
    }

    public void setHeight(float mHeight) {
        this.mHeight = mHeight;
    }

    public void setVisible(boolean visible) {
        mIsShowing = visible;
    }

    public int getColor() {
        return R.color.default_scrim_color;
    }

    public boolean isVisible() {
        return mIsShowing;
    }

    public void setAlpha(float alpha) {
        mAlpha = alpha;
    }

    public float getY() {
        return 0.f;
    }

    public float getAlpha() {
        return mAlpha;
    }
}
