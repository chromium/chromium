// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import androidx.annotation.Px;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.compositor.CompositorViewResizer;

/**
 * This class holds the size of any extension to or even replacement for a keyboard. The height can
 * be used to either compute an offset for bottom bars (e.g. CCTs or PWAs) or to push up the content
 * area.
 */
class KeyboardExtensionViewResizer implements CompositorViewResizer {
    private int mHeight;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    @Override
    public @Px int getHeight() {
        return mHeight;
    }

    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Sets a new extension height and notifies observers if its value changed.
     * @param newKeyboardExtensionHeight The height in pixels.
     */
    void setKeyboardExtensionHeight(@Px int newKeyboardExtensionHeight) {
        if (mHeight == newKeyboardExtensionHeight) return;
        mHeight = newKeyboardExtensionHeight;
        for (Observer observer : mObservers) observer.onHeightChanged(mHeight);
    }
}
