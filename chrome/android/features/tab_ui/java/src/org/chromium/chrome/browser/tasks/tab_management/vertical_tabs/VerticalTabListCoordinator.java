// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/** Coordinator to manage and display the Vertical Tab List. */
@NullMarked
public class VerticalTabListCoordinator {
    private final ViewGroup mContainerView;

    public VerticalTabListCoordinator(
            Activity activity,
            NullableObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {

        mContainerView =
                (ViewGroup)
                        LayoutInflater.from(activity)
                                .inflate(R.layout.vertical_tab_layout, null, false);
        mContainerView.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        mContainerView.setBackgroundColor(
                SemanticColorUtils.getColorSurfaceContainerHighest(activity));

        // TODO(crbug.com/513622986):
        // 1. Set up SimpleRecyclerViewAdapter and register the corresponding ViewBinder for
        // Vertical Tabs.
        // 2. Instantiate TabListMediator and bind with TabModelSelectorSupplier.
        // 3. Wire up header container (R.id.vertical_tab_header_container) for search & grid
        // buttons.
        // 4. Wire up footer container (R.id.vertical_tab_footer_container)
        // 5. Attach ItemTouchHelper for vertical row dragging & reordering.
        // 6. Register Right-click / Long-press Context Menu listener for tab interactions.

        // TODO(crbug.com/513622986): Add corresponding unit tests once list adapter, mediator,
        // and view bindings are implemented.
    }

    public View getView() {
        return mContainerView;
    }

    public void destroy() {
        // TODO(crbug.com/513622986): Clean up mediator, observers, and suppliers here.
    }
}
