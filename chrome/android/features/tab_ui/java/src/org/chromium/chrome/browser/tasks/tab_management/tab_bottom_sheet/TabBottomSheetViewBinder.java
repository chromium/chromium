// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import static org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet.TabBottomSheetProperties.FUSEBOX_ENABLED;

import android.view.View;
import android.widget.EditText;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** ViewBinder for the Auto Delete Decision Promo. Connects PropertyModel listeners to the View. */
@NullMarked
public class TabBottomSheetViewBinder {
    /**
     * Binds the given model to the given view.
     *
     * @param model The {@link PropertyModel} to bind.
     * @param view The inflated Android {@link View} of the promo sheet.
     * @param propertyKey The {@link PropertyKey} that changed.
     */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (FUSEBOX_ENABLED == propertyKey) {
            EditText fuseboxEditText = view.findViewById(R.id.fusebox_edit_text);
            fuseboxEditText.setEnabled(model.get(FUSEBOX_ENABLED));
        }
    }
}
