// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.content.Context;
import android.view.LayoutInflater;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.context_sharing.R;

/** Base class for the Tab Bottom Sheet toolbar. */
@NullMarked
public class TabBottomSheetSimpleToolbar extends TabBottomSheetToolbar {
    TabBottomSheetSimpleToolbar(Context context) {
        super(context);
        LayoutInflater.from(this.getContext())
                .inflate(R.layout.tab_bottom_sheet_toolbar, this, true);
    }
}
