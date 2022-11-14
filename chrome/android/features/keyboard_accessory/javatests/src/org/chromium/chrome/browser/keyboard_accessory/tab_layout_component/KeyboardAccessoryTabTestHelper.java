// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.tab_layout_component;

import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;

import android.view.View;

import org.hamcrest.Matcher;

import org.chromium.chrome.browser.flags.ChromeFeatureList;

/**
 * Helpers in this class simplify interactions with the Keyboard Accessory Tab Layout.
 */
public class KeyboardAccessoryTabTestHelper {
    private KeyboardAccessoryTabTestHelper() {}

    public static Matcher<View> isKeyboardAccessoryTabLayout() {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY)) {
            return isAssignableFrom(KeyboardAccessoryButtonGroupView.class);
        }
        return isAssignableFrom(KeyboardAccessoryTabLayoutView.class);
    }
}
