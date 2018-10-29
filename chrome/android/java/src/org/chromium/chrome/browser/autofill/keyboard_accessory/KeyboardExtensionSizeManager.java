// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.keyboard_accessory;

import android.support.annotation.Px;

import org.chromium.base.ObserverList;

/**
 * This class holds the size of any extension to or even replacement for a keyboard.
 * For example, it is used by {@link ManualFillingCoordinator} to provide the combined height of
 * {@link KeyboardAccessoryCoordinator} and {@link AccessorySheetCoordinator}.
 * The height can then be used to either compute an offset for bottom bars (e.g. CCTs or PWAs) or to
 * push up the content area.
 */
public class KeyboardExtensionSizeManager {
    private int mHeight;
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    /**
     * Observers are notified when the size of the keyboard extension changes.
     */
    public interface Observer {
        /**
         * Called when the extension height changes.
         * @param keyboardHeight The new height of the keyboard extension.
         */
        void onKeyboardExtensionHeightChanged(@Px int keyboardHeight);
    }

    /**
     * Returns the height of the keyboard extension.
     * @return A height in pixels.
     */
    public @Px int getKeyboardExtensionHeight() {
        return mHeight;
    }

    /**
     * Registered observers are called whenever the extension size changes until unregistered. Does
     * not guarantee order.
     * @param observer a {@link KeyboardExtensionSizeManager.Observer}.
     */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes a registered observer if present.
     * @param observer a registered {@link KeyboardExtensionSizeManager.Observer}.
     */
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
        for (Observer observer : mObservers) observer.onKeyboardExtensionHeightChanged(mHeight);
    }
}