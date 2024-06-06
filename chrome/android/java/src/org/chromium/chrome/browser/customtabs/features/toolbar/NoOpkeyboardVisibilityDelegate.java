// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import android.content.Context;
import android.view.View;

import org.chromium.ui.KeyboardVisibilityDelegate;

/** A {@link KeyboardVisibilityDelegate} that never shows the keyboard. */
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
    public boolean isKeyboardShowing(Context context, View view) {
        return false;
    }

    @Override
    public void addKeyboardVisibilityListener(KeyboardVisibilityListener listener) {}

    @Override
    public void removeKeyboardVisibilityListener(KeyboardVisibilityListener listener) {}
}
