// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.view.View;

import org.chromium.chrome.browser.ui.signin.SigninUtils;

/**
 * FirstRunActivity variant that fills the whole screen, but displays the content in a dialog-like
 * layout when the available space is large, in a DialogWhenLarge style.
 */
public class TabbedModeFirstRunActivity extends FirstRunActivity {
    @Override
    protected View createContentView() {
        return SigninUtils.wrapInDialogWhenLargeLayout(super.createContentView());
    }

    @Override
    public boolean canUseLandscapeLayout() {
        // TabbedModeFirstRunActivity shows FRE in a dialog that always has the portrait
        // orientation, so never use the landscape layout with that activity.
        return false;
    }
}
