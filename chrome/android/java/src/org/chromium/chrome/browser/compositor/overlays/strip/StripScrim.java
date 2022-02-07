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
    private float mDrawY;
    private float mWidth;
    private boolean mIsShowing;

    public StripScrim(float width) {
        mWidth = width;
        mDrawY = 0.f;
        mDrawX = 0.f;
    }

    public float getX() {
        return mDrawX;
    }

    public void setX(float mDrawX) {
        this.mDrawX = mDrawX;
    }

    public float getY() {
        return mDrawY;
    }

    public float getWidth() {
        return mWidth;
    }

    public void setWidth(float mWidth) {
        this.mWidth = mWidth;
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
}
