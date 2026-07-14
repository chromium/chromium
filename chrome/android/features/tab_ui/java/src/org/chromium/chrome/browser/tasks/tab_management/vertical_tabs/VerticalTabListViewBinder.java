// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.res.Resources;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.appcompat.widget.TooltipCompat;

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
        } else if (VerticalTabListProperties.ON_COLLAPSE_CLICK_LISTENER == propertyKey) {
            @Nullable View collapseButton = view.findViewById(R.id.collapse_button);
            if (collapseButton != null) {
                collapseButton.setOnClickListener(
                        model.get(VerticalTabListProperties.ON_COLLAPSE_CLICK_LISTENER));
            }
        } else if (VerticalTabListProperties.IS_COLLAPSED == propertyKey) {
            updateCollapsedState(model.get(VerticalTabListProperties.IS_COLLAPSED), view);
        }
    }

    private static void updateCollapsedState(boolean isCollapsed, View view) {
        View collapseButton = view.findViewById(R.id.collapse_button);
        assert collapseButton != null;
        if (collapseButton instanceof ImageView imageView) {
            imageView.setImageResource(
                    isCollapsed
                            ? R.drawable.vertical_tabs_menu_expand
                            : R.drawable.vertical_tabs_menu_collapse);
            int resId =
                    isCollapsed
                            ? R.string.accessibility_expand_vertical_tabs
                            : R.string.accessibility_collapse_vertical_tabs;
            String tooltipText = view.getContext().getString(resId);
            imageView.setContentDescription(tooltipText);
            TooltipCompat.setTooltipText(imageView, tooltipText);
        }

        LinearLayout headerContainer = view.findViewById(R.id.vertical_tab_header_container);
        assert headerContainer != null;
        headerContainer.setOrientation(
                isCollapsed ? LinearLayout.VERTICAL : LinearLayout.HORIZONTAL);
        headerContainer.setGravity(isCollapsed ? Gravity.CENTER_HORIZONTAL : Gravity.NO_GRAVITY);

        Resources res = view.getResources();
        int gap = res.getDimensionPixelSize(R.dimen.vertical_tabs_header_button_gap);
        int collapseMarginEnd =
                res.getDimensionPixelSize(R.dimen.vertical_tabs_header_button_collapsed_margin_end);
        var collapseParams = (ViewGroup.MarginLayoutParams) collapseButton.getLayoutParams();
        collapseParams.setMarginEnd(isCollapsed ? 0 : collapseMarginEnd);
        collapseParams.bottomMargin = isCollapsed ? gap : 0;
        collapseButton.setLayoutParams(collapseParams);

        View gridButton = view.findViewById(R.id.grid_button);
        assert gridButton != null;
        var gridParams = (ViewGroup.MarginLayoutParams) gridButton.getLayoutParams();
        gridParams.setMarginEnd(isCollapsed ? 0 : gap);
        gridParams.bottomMargin = isCollapsed ? gap : 0;
        gridButton.setLayoutParams(gridParams);
        gridButton.setBackgroundResource(
                isCollapsed
                        ? R.drawable.vertical_tabs_top_rounded_button_background
                        : R.drawable.vertical_tabs_left_rounded_button_background);

        View searchButton = view.findViewById(R.id.tab_search_button);
        assert searchButton != null;
        searchButton.setBackgroundResource(
                isCollapsed
                        ? R.drawable.vertical_tabs_bottom_rounded_button_background
                        : R.drawable.vertical_tabs_right_rounded_button_background);

        View spacer = view.findViewById(R.id.header_spacer);
        assert spacer != null;
        spacer.setVisibility(isCollapsed ? View.GONE : View.VISIBLE);
    }
}
