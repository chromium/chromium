// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListProperties.EMPTY_STATE_VISIBLE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListProperties.ENABLE_CONTAINMENT;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListProperties.IS_TABLET_OR_LANDSCAPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListProperties.ON_IS_SCROLLED_CHANGED;
import static org.chromium.chrome.browser.tasks.tab_management.TabGroupListProperties.SYNC_ENABLED;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Forwards changed property values to the view. */
@NullMarked
public class TabGroupListViewBinder {
    /** Stateless propagation of properties. */
    public static void bind(PropertyModel model, TabGroupListView view, PropertyKey propertyKey) {
        if (propertyKey == ON_IS_SCROLLED_CHANGED) {
            view.setOnIsScrolledChanged(model.get(ON_IS_SCROLLED_CHANGED));
        } else if (propertyKey == ENABLE_CONTAINMENT) {
            view.setEnableContainment(model.get(ENABLE_CONTAINMENT));
        } else if (propertyKey == EMPTY_STATE_VISIBLE) {
            view.setEmptyStateVisible(model.get(EMPTY_STATE_VISIBLE));
        } else if (propertyKey == SYNC_ENABLED) {
            view.setSyncEnabled(model.get(SYNC_ENABLED));
        } else if (propertyKey == IS_TABLET_OR_LANDSCAPE) {
            view.maybeMakeSpaceForSearchBar(model.get(IS_TABLET_OR_LANDSCAPE));
        }
    }
}
