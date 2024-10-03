// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tab_ui.TabUiThemeUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

/** {@link org.chromium.ui.modelutil.SimpleRecyclerViewMcp.ViewBinder} for tab List. */
class TabListViewBinder {
    /**
     * Main entrypoint for binding TabListView
     *
     * @param view The view to bind to.
     * @param model The model to bind.
     */
    public static void bindTab(
            PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
        assert view instanceof ViewLookupCachingFrameLayout;
        @TabActionState Integer tabActionState = model.get(TabProperties.TAB_ACTION_STATE);
        if (tabActionState == null) {
            assert false : "TAB_ACTION_STATE must be set before initial bindTab call.";
            return;
        }

        ((TabListView) view).setTabActionState(tabActionState);
        bindListTab(model, (ViewLookupCachingFrameLayout) view, propertyKey);
        if (tabActionState == TabActionState.CLOSABLE) {
            bindClosableListTab(model, (ViewLookupCachingFrameLayout) view, propertyKey);
        } else if (tabActionState == TabActionState.SELECTABLE) {
            bindSelectableListTab(model, (ViewLookupCachingFrameLayout) view, propertyKey);
        } else {
            assert false : "Unsupported TabActionState provided to bindTab.";
        }
    }

    /**
     * Handles any cleanup for recycled views that might be expensive to keep around in the pool.
     *
     * @param model The property model to possibly cleanup.
     * @param view The view to possibly cleanup.
     */
    public static void onViewRecycled(PropertyModel model, View view) {
        if (view instanceof TabListView tabListView) {
            ImageView faviconView = tabListView.findViewById(R.id.start_icon);
            faviconView.setImageDrawable(null);

            FrameLayout container = tabListView.findViewById(R.id.after_title_container);
            TabCardViewBinderUtils.detachTabGroupColorView(container);
        }
    }

    // TODO(crbug.com/40107066): Merge with TabGridViewBinder for shared properties.
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
            fetcher.fetch(
                    tabFavicon -> {
                        if (fetcher != model.get(TabProperties.FAVICON_FETCHER)) return;

                        setFavicon(view, tabFavicon.getDefaultDrawable());
                    });
        } else if (TabProperties.IS_SELECTED == propertyKey) {
            boolean isSelected = model.get(TabProperties.IS_SELECTED);
            boolean isIncognito = model.get(TabProperties.IS_INCOGNITO);
            updateColors(view, isIncognito, isSelected);

            @DrawableRes
            int selectedTabBackground =
                    isIncognito
                            ? R.drawable.selected_tab_background_incognito
                            : R.drawable.selected_tab_background;
            Resources res = view.getResources();
            Resources.Theme theme = view.getContext().getTheme();
            Drawable drawable =
                    new InsetDrawable(
                            ResourcesCompat.getDrawable(res, selectedTabBackground, theme),
                            (int) res.getDimension(R.dimen.tab_list_selected_inset_low_end));
            view.setForeground(isSelected ? drawable : null);
        } else if (TabProperties.URL_DOMAIN == propertyKey) {
            String domain = model.get(TabProperties.URL_DOMAIN);
            ((TextView) view.findViewById(R.id.description)).setText(domain);
        } else if (TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER == propertyKey) {
            @Nullable
            TabGroupColorViewProvider provider =
                    model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER);
            FrameLayout container = view.findViewById(R.id.after_title_container);
            TabCardViewBinderUtils.updateTabGroupColorView(container, provider);
        } else if (TabProperties.TAB_ACTION_BUTTON_DATA == propertyKey) {
            @Nullable TabActionButtonData data = model.get(TabProperties.TAB_ACTION_BUTTON_DATA);
            @Nullable
            TabActionListener tabActionListener = data == null ? null : data.tabActionListener;
            ImageView actionButton = view.findViewById(R.id.end_button);
            TabGridViewBinder.setNullableClickListener(tabActionListener, actionButton, model);

            if (data == null) return;

            Resources res = view.getResources();
            if (data.type == TabActionButtonType.OVERFLOW) {
                actionButton.setImageDrawable(
                        ResourcesCompat.getDrawable(
                                res, R.drawable.ic_more_vert_24dp, view.getContext().getTheme()));
            } else if (data.type == TabActionButtonType.CLOSE) {
                int closeButtonSize = (int) res.getDimension(R.dimen.tab_grid_close_button_size);
                Bitmap bitmap = BitmapFactory.decodeResource(res, R.drawable.btn_close);
                Bitmap.createScaledBitmap(bitmap, closeButtonSize, closeButtonSize, true);
                actionButton.setImageBitmap(bitmap);
            } else if (data.type == TabActionButtonType.SELECT) {
                // Intentional no-op. Handled as part of setTabActionState.
            } else {
                assert false : "Not reached";
            }
        } else if (TabProperties.TAB_CLICK_LISTENER == propertyKey) {
            TabGridViewBinder.setNullableClickListener(
                    model.get(TabProperties.TAB_CLICK_LISTENER), view, model);
        } else if (TabProperties.TAB_LONG_CLICK_LISTENER == propertyKey) {
            TabGridViewBinder.setNullableLongClickListener(
                    model.get(TabProperties.TAB_LONG_CLICK_LISTENER), view, model);
        }
    }

    /**
     * Bind a closable tab to view.
     *
     * @param model The model to bind.
     * @param view The view to bind to.
     * @param propertyKey The property that changed.
     */
    private static void bindClosableListTab(
            PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
        bindListTab(model, view, propertyKey);

        if (TabProperties.IS_SELECTED == propertyKey
                || TabProperties.TAB_ACTION_BUTTON_DATA == propertyKey) {
            ImageView closeButton = view.findViewById(R.id.end_button);
            ImageViewCompat.setImageTintList(
                    closeButton,
                    TabUiThemeProvider.getActionButtonTintList(
                            view.getContext(),
                            model.get(TabProperties.IS_INCOGNITO),
                            /* isSelected= */ false));
        } else if (TabProperties.ACTION_BUTTON_DESCRIPTION_STRING == propertyKey) {
            view.findViewById(R.id.end_button)
                    .setContentDescription(
                            model.get(TabProperties.ACTION_BUTTON_DESCRIPTION_STRING));
        }
    }

    /**
     * Bind color updates.
     *
     * @param view The root view of the item (either Selectable/ClosableTabListView).
     * @param isIncognito Whether the model is in incognito mode.
     * @param isSelected Whether the item is selected.
     */
    private static void updateColors(ViewGroup view, boolean isIncognito, boolean isSelected) {
        // TODO(crbug.com/40272756): isSelected is ignored as the selected row is only outlined not
        // colored so it should use the unselected color. This will be addressed in a fixit.

        // Shared by both classes, from tab_list_card_item.
        View contentView = view.findViewById(R.id.content_view);
        contentView.getBackground().mutate();
        final @ColorInt int backgroundColor =
                TabUiThemeUtils.getCardViewBackgroundColor(
                        view.getContext(), isIncognito, /* isSelected= */ false);
        ViewCompat.setBackgroundTintList(contentView, ColorStateList.valueOf(backgroundColor));

        final @ColorInt int textColor =
                TabUiThemeUtils.getTitleTextColor(
                        view.getContext(), isIncognito, /* isSelected= */ false);
        TextView titleView = view.findViewById(R.id.title);
        TextView descriptionView = view.findViewById(R.id.description);
        titleView.setTextColor(textColor);
        descriptionView.setTextColor(textColor);

        ImageView faviconView = view.findViewById(R.id.start_icon);
        if (faviconView.getBackground() == null) {
            faviconView.setBackgroundResource(R.drawable.list_item_icon_modern_bg);
        }
        faviconView.getBackground().mutate();
        final @ColorInt int faviconBackgroundColor =
                TabUiThemeUtils.getMiniThumbnailPlaceholderColor(
                        view.getContext(), isIncognito, /* isSelected= */ false);
        ViewCompat.setBackgroundTintList(
                faviconView, ColorStateList.valueOf(faviconBackgroundColor));
    }

    /**
     * Bind a selectable tab to view.
     *
     * @param model The model to bind.
     * @param view The view to bind to.
     * @param propertyKey The property that changed.
     */
    private static void bindSelectableListTab(
            PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
        bindListTab(model, view, propertyKey);

        final int tabId = model.get(TabProperties.TAB_ID);
        TabListView tabListView = (TabListView) view;
        if (TabProperties.TAB_SELECTION_DELEGATE == propertyKey) {
            tabListView.setSelectionDelegate(model.get(TabProperties.TAB_SELECTION_DELEGATE));
            tabListView.setItem(tabId);
        } else if (TabProperties.IS_SELECTED == propertyKey
                || TabProperties.TAB_ACTION_BUTTON_DATA == propertyKey) {
            boolean isSelected = model.get(TabProperties.IS_SELECTED);
            boolean isIncognito = model.get(TabProperties.IS_INCOGNITO);
            ImageView actionButton = view.findViewById(R.id.end_button);

            Context context = view.getContext();
            Resources res = view.getResources();
            int level = TabCardViewBinderUtils.getCheckmarkLevel(res, isSelected);
            ColorStateList backgroundColorStateList =
                    getBackgroundColorStateList(context, isSelected, isIncognito);

            var background = actionButton.getBackground();
            background.setLevel(level);
            DrawableCompat.setTintList(background.mutate(), backgroundColorStateList);

            // The check should be invisible if not selected.
            actionButton.getDrawable().setAlpha(isSelected ? 255 : 0);
            ImageViewCompat.setImageTintList(
                    actionButton,
                    isSelected ? getCheckedDrawableColorStateList(context, isIncognito) : null);
            if (isSelected) ((AnimatedVectorDrawableCompat) actionButton.getDrawable()).start();
        }
    }

    private static void setFavicon(View view, Drawable favicon) {
        ImageView faviconView = view.findViewById(R.id.start_icon);
        faviconView.setImageDrawable(favicon);
    }

    private static ColorStateList getCheckedDrawableColorStateList(
            Context context, boolean isIncognito) {
        return ColorStateList.valueOf(
                isIncognito
                        ? context.getColor(R.color.default_icon_color_dark)
                        : SemanticColorUtils.getDefaultIconColorInverse(context));
    }

    private static ColorStateList getBackgroundColorStateList(
            Context context, boolean isSelected, boolean isIncognito) {
        if (isSelected) {
            return ColorStateList.valueOf(
                    isIncognito
                            ? context.getColor(R.color.baseline_primary_80)
                            : SemanticColorUtils.getDefaultControlColorActive(context));
        } else {
            return AppCompatResources.getColorStateList(
                    context,
                    isIncognito
                            ? R.color.default_icon_color_light
                            : R.color.default_icon_color_tint_list);
        }
    }
}
