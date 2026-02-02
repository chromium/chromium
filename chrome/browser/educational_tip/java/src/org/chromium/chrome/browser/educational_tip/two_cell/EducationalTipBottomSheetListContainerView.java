// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import android.content.Context;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;

/**
 * The container view holding multiple {@link EducationalTipBottomSheetListItemView} in a bottom
 * sheet.
 */
@NullMarked
public class EducationalTipBottomSheetListContainerView extends LinearLayout {
    // TODO(crbug.com/479597724): Implement container view.

    public EducationalTipBottomSheetListContainerView(Context context) {
        super(context);
    }

    /** Adds list items views to this container view. */
    public void renderSetUpList() {
        // TODO(crbug.com/479597724): Iterate through setup list items and creating list item view.
        addView(new EducationalTipBottomSheetListItemView(getContext()));
    }
}
