// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.archived_tabs_auto_delete_promo;

import android.view.View.OnClickListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the "Auto Delete Archived Tabs Decision Promo" bottom sheet. */
@NullMarked
public class ArchivedTabsAutoDeletePromoProperties {
    // Text Properties
    public static final WritableObjectPropertyKey<String> PROMO_DESCRIPTION_STRING =
            new WritableObjectPropertyKey<>("promo_description_string");
    // Click Listener Properties
    public static final WritableObjectPropertyKey<OnClickListener> ON_YES_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>("on_yes_click");
    public static final WritableObjectPropertyKey<OnClickListener> ON_NO_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>("on_no_click");

    public static final PropertyKey[] ALL_KEYS = {
        PROMO_DESCRIPTION_STRING, ON_YES_BUTTON_CLICK_LISTENER, ON_NO_BUTTON_CLICK_LISTENER
    };

    /**
     * Creates a default model structure. Listeners will be populated by the Coordinator.
     *
     * @return A new {@link PropertyModel} instance.
     */
    public static PropertyModel createDefaultModel() {
        return new PropertyModel.Builder(ALL_KEYS).build();
    }
}
