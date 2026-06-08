// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the Vertical Tab List. */
@NullMarked
public class VerticalTabListViewBinder {
    /**
     * Binds the given model to the view.
     *
     * @param model The model to bind.
     * @param view The container view.
     * @param propertyKey The key of the property that changed.
     */
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (VerticalTabListProperties.ON_GRID_CLICK_LISTENER == propertyKey) {
            @Nullable View gridButton = view.findViewById(R.id.grid_button);
            if (gridButton != null) {
                gridButton.setOnClickListener(
                        model.get(VerticalTabListProperties.ON_GRID_CLICK_LISTENER));
            }
        } else if (VerticalTabListProperties.ON_SEARCH_CLICK_LISTENER == propertyKey) {
            @Nullable View searchButton = view.findViewById(R.id.tab_search_button);
            if (searchButton != null) {
                searchButton.setOnClickListener(
                        model.get(VerticalTabListProperties.ON_SEARCH_CLICK_LISTENER));
            }
        } else if (VerticalTabListProperties.ON_NEW_TAB_CLICK_LISTENER == propertyKey) {
            @Nullable View newTabButton = view.findViewById(R.id.new_tab_button);
            if (newTabButton != null) {
                newTabButton.setOnClickListener(
                        model.get(VerticalTabListProperties.ON_NEW_TAB_CLICK_LISTENER));
            }
        }
    }
}
