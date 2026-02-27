// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipBottomSheetProperties.BOTTOM_SHEET_DESCRIPTION;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipBottomSheetProperties.BOTTOM_SHEET_LIST_ITEMS;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipBottomSheetProperties.BOTTOM_SHEET_LIST_ITEMS_ON_CLICK;
import static org.chromium.chrome.browser.educational_tip.two_cell.EducationalTipBottomSheetProperties.BOTTOM_SHEET_TITLE;

import android.view.View;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.educational_tip.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for a generic two-cell educational tip bottom sheet. */
@NullMarked
public class EducationalTipBottomSheetViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == BOTTOM_SHEET_TITLE) {
            TextView title = view.findViewById(R.id.setup_list_bottom_sheet_title);
            title.setText(model.get(BOTTOM_SHEET_TITLE));
        } else if (propertyKey == BOTTOM_SHEET_DESCRIPTION) {
            TextView description = view.findViewById(R.id.setup_list_bottom_sheet_description);
            description.setText(model.get(BOTTOM_SHEET_DESCRIPTION));
        } else if (propertyKey == BOTTOM_SHEET_LIST_ITEMS) {
            EducationalTipBottomSheetListContainerView listContainerView =
                    view.findViewById(R.id.setup_list_bottom_sheet_container_view);
            listContainerView.renderSetUpList(model.get(BOTTOM_SHEET_LIST_ITEMS));
        } else if (propertyKey == BOTTOM_SHEET_LIST_ITEMS_ON_CLICK) {
            EducationalTipBottomSheetListContainerView listContainerView =
                    view.findViewById(R.id.setup_list_bottom_sheet_container_view);
            listContainerView.setDismissBottomSheet(model.get(BOTTOM_SHEET_LIST_ITEMS_ON_CLICK));
        }
    }
}
