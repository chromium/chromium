// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.build.annotations.NullMarked;

/** Abstract class for Tab Bottom Sheet toolbars. */
@NullMarked
public abstract class TabBottomSheetToolbar extends FrameLayout {
    TabBottomSheetToolbar(Context context) {
        super(context);
    }

    View getToolbarView() {
        return this;
    }
}
