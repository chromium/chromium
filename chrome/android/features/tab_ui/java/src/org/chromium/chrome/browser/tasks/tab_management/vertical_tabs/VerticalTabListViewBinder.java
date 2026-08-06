// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.view.View;
import android.widget.ImageView;

import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabUiThemeUtil;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
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
        updateButtonSizes(view);

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
        } else if (VerticalTabListProperties.IS_INCOGNITO == propertyKey) {
            updateIncognitoColors(view, model.get(VerticalTabListProperties.IS_INCOGNITO));
        }
    }

    /**
     * Updates the container background and button icon tints for incognito mode when running in a
     * shared window (e.g. foldables and phones). If incognito runs as a separate window (e.g. on
     * tablets), this is a no-op as the activity theme already provides incognito styling.
     *
     * @param view The root {@link VerticalTabRailLayout} container view.
     * @param isIncognito Whether the active tab model is incognito branded.
     */
    private static void updateIncognitoColors(VerticalTabRailLayout view, boolean isIncognito) {
        if (IncognitoUtils.shouldOpenIncognitoAsWindow()) {
            return;
        }
        Context context = view.getContext();
        int backgroundColor = TabUiThemeUtil.getTabStripBackgroundColor(context, isIncognito);
        view.setBackgroundColor(backgroundColor);

        ColorStateList iconTint =
                isIncognito
                        ? context.getColorStateList(R.color.incognito_tab_action_button_color)
                        : ColorStateList.valueOf(SemanticColorUtils.getDefaultIconColor(context));

        @Nullable ImageView collapseButton = view.findViewById(R.id.collapse_button);
        if (collapseButton != null) {
            ImageViewCompat.setImageTintList(collapseButton, iconTint);
        }

        @Nullable ImageView gridButton = view.findViewById(R.id.grid_button);
        if (gridButton != null) {
            ImageViewCompat.setImageTintList(gridButton, iconTint);
        }

        @Nullable ImageView searchButton = view.findViewById(R.id.tab_search_button);
        if (searchButton != null) {
            ImageViewCompat.setImageTintList(searchButton, iconTint);
        }

        @Nullable ImageView newTabButton = view.findViewById(R.id.new_tab_button);
        if (newTabButton != null) {
            ImageViewCompat.setImageTintList(newTabButton, iconTint);
        }

        @Nullable ColorStateList buttonBgTint =
                isIncognito
                        ? context.getColorStateList(
                                R.color.incognito_vertical_tabs_button_background_color)
                        : null;

        if (gridButton != null) {
            ViewCompat.setBackgroundTintList(gridButton, buttonBgTint);
        }
        if (searchButton != null) {
            ViewCompat.setBackgroundTintList(searchButton, buttonBgTint);
        }
        if (newTabButton != null) {
            ViewCompat.setBackgroundTintList(newTabButton, buttonBgTint);
        }
    }

    /**
     * Updates the dimensions of header and new tab buttons based on device form factor.
     *
     * @param view The root {@link VerticalTabRailLayout} container view.
     */
    public static void updateButtonSizes(VerticalTabRailLayout view) {
        Resources res = view.getResources();
        int buttonSize = res.getDimensionPixelSize(R.dimen.vertical_tabs_header_button_size);
        int newTabHeight = res.getDimensionPixelSize(R.dimen.vertical_tabs_new_tab_button_height);

        View collapseButton = view.findViewById(R.id.collapse_button);
        if (collapseButton != null && collapseButton.getLayoutParams() != null) {
            var params = collapseButton.getLayoutParams();
            if (params.width != buttonSize || params.height != buttonSize) {
                params.width = buttonSize;
                params.height = buttonSize;
                collapseButton.setLayoutParams(params);
            }
        }

        View gridButton = view.findViewById(R.id.grid_button);
        if (gridButton != null && gridButton.getLayoutParams() != null) {
            var params = gridButton.getLayoutParams();
            if (params.width != buttonSize || params.height != buttonSize) {
                params.width = buttonSize;
                params.height = buttonSize;
                gridButton.setLayoutParams(params);
            }
        }

        View searchButton = view.findViewById(R.id.tab_search_button);
        if (searchButton != null && searchButton.getLayoutParams() != null) {
            var params = searchButton.getLayoutParams();
            if (params.width != buttonSize || params.height != buttonSize) {
                params.width = buttonSize;
                params.height = buttonSize;
                searchButton.setLayoutParams(params);
            }
        }

        View newTabButton = view.findViewById(R.id.new_tab_button);
        if (newTabButton != null && newTabButton.getLayoutParams() != null) {
            var params = newTabButton.getLayoutParams();
            if (params.height != newTabHeight) {
                params.height = newTabHeight;
                newTabButton.setLayoutParams(params);
            }
        }
    }
}
