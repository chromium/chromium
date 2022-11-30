// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * {@link org.chromium.ui.modelutil.SimpleRecyclerViewMcp.ViewBinder} for tab List.
 */
class TabListViewBinder {
    // TODO(1023557): Merge with TabGridViewBinder for shared properties.
    public static void bindListTab(
            PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
        View fastView = view;

        if (TabProperties.TITLE == propertyKey) {
            String title = model.get(TabProperties.TITLE);
            ((TextView) fastView.findViewById(R.id.title)).setText(title);
        } else if (TabProperties.FAVICON == propertyKey) {
            Drawable favicon = model.get(TabProperties.FAVICON).getDefaultDrawable();
            ImageView faviconView = (ImageView) fastView.findViewById(R.id.start_icon);
            faviconView.setBackgroundResource(R.drawable.list_item_icon_modern_bg);
            faviconView.setImageDrawable(favicon);
        } else if (TabProperties.TAB_CLOSED_LISTENER == propertyKey) {
            if (model.get(TabProperties.TAB_CLOSED_LISTENER) == null) {
                fastView.findViewById(R.id.end_button).setOnClickListener(null);
            } else {
                fastView.findViewById(R.id.end_button).setOnClickListener(v -> {
                    int tabId = model.get(TabProperties.TAB_ID);
                    model.get(TabProperties.TAB_CLOSED_LISTENER).run(tabId);
                });
            }
        } else if (TabProperties.IS_SELECTED == propertyKey) {
            int selectedTabBackground =
                    model.get(TabProperties.SELECTED_TAB_BACKGROUND_DRAWABLE_ID);
            Resources res = view.getResources();
            Resources.Theme theme = view.getContext().getTheme();
            Drawable drawable = new InsetDrawable(
                    ResourcesCompat.getDrawable(res, selectedTabBackground, theme),
                    (int) res.getDimension(R.dimen.tab_list_selected_inset_low_end));
            view.setForeground(model.get(TabProperties.IS_SELECTED) ? drawable : null);
        } else if (TabProperties.TAB_SELECTED_LISTENER == propertyKey) {
            if (model.get(TabProperties.TAB_SELECTED_LISTENER) == null) {
                view.setOnClickListener(null);
            } else {
                view.setOnClickListener(v -> {
                    int tabId = model.get(TabProperties.TAB_ID);
                    model.get(TabProperties.TAB_SELECTED_LISTENER).run(tabId);
                });
            }
        } else if (TabProperties.URL_DOMAIN == propertyKey) {
            String domain = model.get(TabProperties.URL_DOMAIN);
            ((TextView) fastView.findViewById(R.id.description)).setText(domain);
        }
    }

    /**
     * Bind a selectable tab to view.
     * @param model The model to bind.
     * @param view The view to bind to.
     * @param propertyKey The property that changed.
     */
    public static void bindSelectableListTab(
            PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
        bindListTab(model, view, propertyKey);

        final int tabId = model.get(TabProperties.TAB_ID);
        final int defaultLevel = view.getResources().getInteger(R.integer.list_item_level_default);
        final int selectedLevel =
                view.getResources().getInteger(R.integer.list_item_level_selected);
        SelectableTabGridView selectableTabListView = view.findViewById(R.id.content_view);

        if (TabProperties.SELECTABLE_TAB_CLICKED_LISTENER == propertyKey) {
            View.OnClickListener onClickListener = v -> {
                model.get(TabProperties.SELECTABLE_TAB_CLICKED_LISTENER).run(tabId);
                selectableTabListView.onClick();
            };
            View.OnLongClickListener onLongClickListener = v -> {
                model.get(TabProperties.SELECTABLE_TAB_CLICKED_LISTENER).run(tabId);
                return selectableTabListView.onLongClick(selectableTabListView);
            };
            selectableTabListView.setOnClickListener(onClickListener);
            selectableTabListView.setOnLongClickListener(onLongClickListener);

            ImageView endButton = selectableTabListView.findViewById(R.id.end_button);
            endButton.setOnClickListener(onClickListener);
            endButton.setOnLongClickListener(onLongClickListener);
        } else if (TabProperties.TAB_SELECTION_DELEGATE == propertyKey) {
            assert model.get(TabProperties.TAB_SELECTION_DELEGATE) != null;
            selectableTabListView.setSelectionDelegate(
                    model.get(TabProperties.TAB_SELECTION_DELEGATE));
            selectableTabListView.setItem(tabId);
        } else if (TabProperties.IS_SELECTED == propertyKey) {
            boolean isSelected = model.get(TabProperties.IS_SELECTED);
            ImageView actionButton = (ImageView) view.findViewById(R.id.end_button);
            actionButton.getBackground().setLevel(isSelected ? selectedLevel : defaultLevel);
            DrawableCompat.setTintList(actionButton.getBackground().mutate(),
                    isSelected ? model.get(
                            TabProperties.SELECTABLE_TAB_ACTION_BUTTON_SELECTED_BACKGROUND)
                               : model.get(TabProperties.SELECTABLE_TAB_ACTION_BUTTON_BACKGROUND));

            // The check should be invisible if not selected.
            actionButton.getDrawable().setAlpha(isSelected ? 255 : 0);
            ApiCompatibilityUtils.setImageTintList(actionButton,
                    isSelected ? model.get(TabProperties.CHECKED_DRAWABLE_STATE_LIST) : null);
            if (isSelected) ((AnimatedVectorDrawableCompat) actionButton.getDrawable()).start();
        }
    }
}
