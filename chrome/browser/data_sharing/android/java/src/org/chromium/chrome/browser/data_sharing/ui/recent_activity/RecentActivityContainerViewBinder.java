// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** View binder for the recent activity list container view. */
@NullMarked
class RecentActivityContainerViewBinder {

    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == RecentActivityContainerProperties.EMPTY_STATE_VISIBLE) {
            boolean isEmptyState = model.get(RecentActivityContainerProperties.EMPTY_STATE_VISIBLE);
            View emptyView = view.findViewById(R.id.recent_activity_empty_view);
            View recyclerView = view.findViewById(R.id.recent_activity_recycler_view);
            emptyView.setVisibility(isEmptyState ? View.VISIBLE : View.GONE);
            recyclerView.setVisibility(isEmptyState ? View.GONE : View.VISIBLE);
        } else if (propertyKey == RecentActivityContainerProperties.MENU_CLICK_LISTENER) {
            ListMenuButton menuButton = view.findViewById(R.id.recent_activity_menu_button);
            menuButton.setOnClickListener(
                    model.get(RecentActivityContainerProperties.MENU_CLICK_LISTENER));
        }
    }
}
