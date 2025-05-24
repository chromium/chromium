// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.archived_tabs_auto_delete_promo;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

/** ViewBinder for the Auto Delete Decision Promo. Connects PropertyModel listeners to the View. */
@NullMarked
public class ArchivedTabsAutoDeletePromoViewBinder {
    /**
     * Binds the given model to the given view.
     *
     * @param model The {@link PropertyModel} to bind.
     * @param view The inflated Android {@link View} of the promo sheet.
     * @param propertyKey The {@link PropertyKey} that changed.
     */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (ArchivedTabsAutoDeletePromoProperties.ON_YES_BUTTON_CLICK_LISTENER == propertyKey) {
            ButtonCompat yesButton = view.findViewById(R.id.promo_yes_button);
            yesButton.setOnClickListener(
                    model.get(ArchivedTabsAutoDeletePromoProperties.ON_YES_BUTTON_CLICK_LISTENER));
        } else if (ArchivedTabsAutoDeletePromoProperties.ON_NO_BUTTON_CLICK_LISTENER
                == propertyKey) {
            ButtonCompat noButton = view.findViewById(R.id.promo_no_button);
            noButton.setOnClickListener(
                    model.get(ArchivedTabsAutoDeletePromoProperties.ON_NO_BUTTON_CLICK_LISTENER));
        }
    }
}
