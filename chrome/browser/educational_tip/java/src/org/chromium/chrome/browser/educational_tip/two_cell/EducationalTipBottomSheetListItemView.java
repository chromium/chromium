// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import android.content.Context;
import android.widget.RelativeLayout;

import org.chromium.build.annotations.NullMarked;

/**
 * The list item view within a {@link EducationalTipBottomSheetListContainerView} that is in a
 * bottom sheet.
 */
@NullMarked
public class EducationalTipBottomSheetListItemView extends RelativeLayout {
    public EducationalTipBottomSheetListItemView(Context context) {
        super(context);
    }

    // TODO(crbug.com/479597724): Implement title, description, icon, and button for a list item.
}
