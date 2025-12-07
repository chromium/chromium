// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.KeyboardVisibilityDelegate;

/** A {@link KeyboardVisibilityDelegate} that never shows the keyboard. */
@NullMarked
class NoOpkeyboardVisibilityDelegate extends KeyboardVisibilityDelegate {
    @Override
    public void showKeyboard(View view) {}

    @Override
    public boolean hideKeyboard(View view) {
        return false;
    }

    @Override
    public int calculateTotalKeyboardHeight(View view) {
        return 0;
    }

    @Override
    public boolean isKeyboardShowing(View view) {
        return false;
    }

    @Override
    public void addKeyboardVisibilityListener(KeyboardVisibilityListener listener) {}

    @Override
    public void removeKeyboardVisibilityListener(KeyboardVisibilityListener listener) {}
}
