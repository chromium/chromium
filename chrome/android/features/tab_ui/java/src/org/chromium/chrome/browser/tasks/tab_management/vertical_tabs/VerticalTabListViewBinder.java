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
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
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
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (VerticalTabListProperties.ON_GRID_CLICK_LISTENER == propertyKey) {
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
            updateCollapsedState(model.get(VerticalTabListProperties.COLLAPSE_STATE), view);
        }
    }

    private static void updateCollapsedState(@RailCollapseState int railCollapseState, View view) {
        boolean isCollapsed = railCollapseState == RailCollapseState.COLLAPSED;
        boolean isManuallyExpanded = railCollapseState == RailCollapseState.EXPANDED;

        View collapseButton = view.findViewById(R.id.collapse_button);
        assert collapseButton != null;
        if (collapseButton instanceof ImageView imageView) {
            imageView.setImageResource(
                    isManuallyExpanded
                            ? R.drawable.vertical_tabs_menu_collapse
                            : R.drawable.vertical_tabs_menu_expand);
            int resId =
                    isManuallyExpanded
                            ? R.string.accessibility_collapse_vertical_tabs
                            : R.string.accessibility_expand_vertical_tabs;
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
        var collapseParams = (ViewGroup.MarginLayoutParams) collapseButton.getLayoutParams();
        collapseParams.setMarginEnd(
                isCollapsed
                        ? 0
                        : res.getDimensionPixelSize(
                                R.dimen.vertical_tabs_header_button_collapsed_margin_end));
        collapseParams.bottomMargin =
                isCollapsed
                        ? res.getDimensionPixelOffset(R.dimen.vertical_tabs_header_padding_vertical)
                        : 0;
        collapseButton.setLayoutParams(collapseParams);

        int gap = res.getDimensionPixelSize(R.dimen.vertical_tabs_header_button_gap);
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

        View newTabButton = view.findViewById(R.id.new_tab_button);
        assert newTabButton != null;
        var newTabParams = (ViewGroup.LayoutParams) newTabButton.getLayoutParams();
        newTabParams.width =
                isCollapsed
                        ? res.getDimensionPixelSize(R.dimen.vertical_tabs_header_button_size)
                        : ViewGroup.LayoutParams.MATCH_PARENT;
        newTabParams.height =
                isCollapsed
                        ? res.getDimensionPixelSize(R.dimen.vertical_tabs_header_button_size)
                        : res.getDimensionPixelSize(R.dimen.vertical_tabs_new_tab_button_height);
        newTabButton.setLayoutParams(newTabParams);
    }
}
