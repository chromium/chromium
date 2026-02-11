// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import static org.chromium.chrome.browser.tab_bottom_sheet.TabBottomSheetProperties.FUSEBOX_OFFSET;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.context_sharing.R;
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
        if (FUSEBOX_OFFSET == propertyKey) {
            View fuseboxContainer = view.findViewById(R.id.fusebox_container);
            float offset = -(view.getHeight() - model.get(FUSEBOX_OFFSET));
            fuseboxContainer.setTranslationY(offset);
        }
    }
}
