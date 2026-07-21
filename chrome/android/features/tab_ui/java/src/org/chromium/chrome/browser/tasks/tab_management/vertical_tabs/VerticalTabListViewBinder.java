// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
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
    public static void bind(
            PropertyModel model, VerticalTabRailLayout view, PropertyKey propertyKey) {
        if (VerticalTabListProperties.EXPAND_OR_COLLAPSE_ON_HOVER_LISTENER == propertyKey) {
            view.setExpandOrCollapseOnHoverListener(
                    model.get(VerticalTabListProperties.EXPAND_OR_COLLAPSE_ON_HOVER_LISTENER));
        } else if (VerticalTabListProperties.ON_GRID_CLICK_LISTENER == propertyKey) {
            View gridButton = view.findViewById(R.id.grid_button);
            assert gridButton != null;
            gridButton.setOnClickListener(
                    model.get(VerticalTabListProperties.ON_GRID_CLICK_LISTENER));
        } else if (VerticalTabListProperties.ON_SEARCH_CLICK_LISTENER == propertyKey) {
            View searchButton = view.findViewById(R.id.tab_search_button);
            assert searchButton != null;
            searchButton.setOnClickListener(
                    model.get(VerticalTabListProperties.ON_SEARCH_CLICK_LISTENER));
        } else if (VerticalTabListProperties.ON_NEW_TAB_CLICK_LISTENER == propertyKey) {
            View newTabButton = view.findViewById(R.id.new_tab_button);
            assert newTabButton != null;
            newTabButton.setOnClickListener(
                    model.get(VerticalTabListProperties.ON_NEW_TAB_CLICK_LISTENER));
        } else if (VerticalTabListProperties.ON_COLLAPSE_CLICK_LISTENER == propertyKey) {
            View collapseButton = view.findViewById(R.id.collapse_button);
            assert collapseButton != null;
            collapseButton.setOnClickListener(
                    model.get(VerticalTabListProperties.ON_COLLAPSE_CLICK_LISTENER));
        } else if (VerticalTabListProperties.IS_COLLAPSE_BUTTON_ENABLED == propertyKey) {
            View collapseButton = view.findViewById(R.id.collapse_button);
            assert collapseButton != null;
            boolean enabled = model.get(VerticalTabListProperties.IS_COLLAPSE_BUTTON_ENABLED);
            collapseButton.setEnabled(enabled);
            float disabledAlpha =
                    VerticalTabUtils.getFloatResource(
                            view.getContext(), org.chromium.ui.R.dimen.default_disabled_alpha);
            collapseButton.setAlpha(enabled ? 1.0f : disabledAlpha);
        } else if (VerticalTabListProperties.COLLAPSE_STATE == propertyKey) {
            view.setCollapseState(model.get(VerticalTabListProperties.COLLAPSE_STATE));
        }
    }
}
