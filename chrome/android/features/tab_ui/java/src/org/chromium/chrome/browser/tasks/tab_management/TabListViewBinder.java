// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * {@link org.chromium.ui.modelutil.SimpleRecyclerViewMcp.ViewBinder} for tab List.
 */
class TabListViewBinder {
    // TODO(1023557): Merge with TabGridViewBinder for shared properties.
    private static void bindListTab(
            PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
        if (TabProperties.TITLE == propertyKey) {
            String title = model.get(TabProperties.TITLE);
            ((TextView) view.findViewById(R.id.title)).setText(title);
        } else if (TabProperties.FAVICON_FETCHER == propertyKey) {
            final TabListFaviconProvider.TabFaviconFetcher fetcher =
                    model.get(TabProperties.FAVICON_FETCHER);
            if (fetcher == null) {
                setFavicon(view, null);
                return;
            }
            fetcher.fetch(tabFavicon -> {
                if (fetcher != model.get(TabProperties.FAVICON_FETCHER)) return;

                setFavicon(view, tabFavicon.getDefaultDrawable());
            });
        } else if (TabProperties.IS_SELECTED == propertyKey) {
            int selectedTabBackground =
                    model.get(TabProperties.SELECTED_TAB_BACKGROUND_DRAWABLE_ID);
            Resources res = view.getResources();
            Resources.Theme theme = view.getContext().getTheme();
            Drawable drawable = new InsetDrawable(
                    ResourcesCompat.getDrawable(res, selectedTabBackground, theme),
                    (int) res.getDimension(R.dimen.tab_list_selected_inset_low_end));
            view.setForeground(model.get(TabProperties.IS_SELECTED) ? drawable : null);
        } else if (TabProperties.IS_INCOGNITO == propertyKey) {
            updateColors(view, model.get(TabProperties.IS_INCOGNITO),
                    model.get(TabProperties.IS_SELECTED));
        } else if (TabProperties.URL_DOMAIN == propertyKey) {
            String domain = model.get(TabProperties.URL_DOMAIN);
            ((TextView) view.findViewById(R.id.description)).setText(domain);
        }
    }

    /**
     * Bind a closable tab to view.
     * @param model The model to bind.
     * @param view The view to bind to.
     * @param propertyKey The property that changed.
     */
    public static void bindClosableListTab(
            PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
        bindListTab(model, view, propertyKey);

        if (TabProperties.IS_INCOGNITO == propertyKey) {
            ImageView closeButton = (ImageView) view.findViewById(R.id.end_button);
            ImageViewCompat.setImageTintList(closeButton,
                    TabUiThemeProvider.getActionButtonTintList(view.getContext(),
                            model.get(TabProperties.IS_INCOGNITO), /*isSelected=*/false));
        } else if (TabProperties.CLOSE_BUTTON_DESCRIPTION_STRING == propertyKey) {
            view.findViewById(R.id.end_button)
                    .setContentDescription(
                            model.get(TabProperties.CLOSE_BUTTON_DESCRIPTION_STRING));
        } else if (TabProperties.TAB_SELECTED_LISTENER == propertyKey) {
            if (model.get(TabProperties.TAB_SELECTED_LISTENER) == null) {
                view.setOnClickListener(null);
            } else {
                view.setOnClickListener(v -> {
                    int tabId = model.get(TabProperties.TAB_ID);
                    model.get(TabProperties.TAB_SELECTED_LISTENER).run(tabId);
                });
            }
        } else if (TabProperties.TAB_CLOSED_LISTENER == propertyKey) {
            if (model.get(TabProperties.TAB_CLOSED_LISTENER) == null) {
                view.findViewById(R.id.end_button).setOnClickListener(null);
            } else {
                view.findViewById(R.id.end_button).setOnClickListener(v -> {
                    int tabId = model.get(TabProperties.TAB_ID);
                    model.get(TabProperties.TAB_CLOSED_LISTENER).run(tabId);
                });
            }
        }
    }

    /**
     * Bind color updates.
     * @param view The root view of the item.
     * @param isIncognito Whether the model is in incognito mode.
     * @param isSelected Whether the item is selected.
     */
    private static void updateColors(ViewGroup view, boolean isIncognito, boolean isSelected) {
        // TODO(crbug.com/1455397): isSelected is ignored as the selected row is only outlined not
        // colored so it should use the unselected color. This will be addressed in a fixit.

        View cardView = view.findViewById(R.id.content_view);
        cardView.getBackground().mutate();
        final @ColorInt int backgroundColor = TabUiThemeProvider.getCardViewBackgroundColor(
                view.getContext(), isIncognito, /*isSelected=*/false);
        ViewCompat.setBackgroundTintList(cardView, ColorStateList.valueOf(backgroundColor));

        final @ColorInt int textColor = TabUiThemeProvider.getTitleTextColor(
                view.getContext(), isIncognito, /*isSelected=*/false);
        TextView titleView = (TextView) view.findViewById(R.id.title);
        TextView descriptionView = (TextView) view.findViewById(R.id.description);
        titleView.setTextColor(textColor);
        descriptionView.setTextColor(textColor);

        ImageView faviconView = (ImageView) view.findViewById(R.id.start_icon);
        if (faviconView.getBackground() == null) {
            faviconView.setBackgroundResource(R.drawable.list_item_icon_modern_bg);
        }
        faviconView.getBackground().mutate();
        final @ColorInt int faviconBackgroundColor =
                TabUiThemeProvider.getMiniThumbnailPlaceholderColor(
                        view.getContext(), isIncognito, /*isSelected=*/false);
        ViewCompat.setBackgroundTintList(
                faviconView, ColorStateList.valueOf(faviconBackgroundColor));
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

            // The row should act as one large button.
            ImageView endButton = selectableTabListView.findViewById(R.id.end_button);
            endButton.setOnClickListener(onClickListener);
            endButton.setOnLongClickListener(onLongClickListener);
            endButton.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
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
            ImageViewCompat.setImageTintList(actionButton,
                    isSelected ? model.get(TabProperties.CHECKED_DRAWABLE_STATE_LIST) : null);
            if (isSelected) ((AnimatedVectorDrawableCompat) actionButton.getDrawable()).start();
        }
    }

    private static void setFavicon(View view, Drawable favicon) {
        ImageView faviconView = (ImageView) view.findViewById(R.id.start_icon);
        faviconView.setImageDrawable(favicon);
    }
}
