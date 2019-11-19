// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.os.Build;
import android.support.graphics.drawable.AnimatedVectorDrawableCompat;
import android.support.v4.content.res.ResourcesCompat;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v4.view.ViewCompat;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.chrome.browser.flags.FeatureUtilities;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageView;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

/**
 * {@link org.chromium.ui.modelutil.SimpleRecyclerViewMcp.ViewBinder} for tab grid.
 * This class supports both full and partial updates to the {@link TabGridViewHolder}.
 */
class TabGridViewBinder {
    /**
     * Bind a closable tab to a view.
     * @param model The model to bind.
     * @param view The view to bind to.
     * @param propertyKey The property that changed.
     */
    public static void bindClosableTab(
            PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
        assert view instanceof ViewLookupCachingFrameLayout;
        if (propertyKey == null) {
            onBindAll((ViewLookupCachingFrameLayout) view, model, TabProperties.UiType.CLOSABLE);
            return;
        }
        bindCommonProperties(model, (ViewLookupCachingFrameLayout) view, propertyKey);
        bindClosableTabProperties(model, (ViewLookupCachingFrameLayout) view, propertyKey);
    }

    /**
     * Bind a selectable tab to a view.
     * @param model The model to bind.
     * @param view The view to bind to.
     * @param propertyKey The property that changed.
     */
    public static void bindSelectableTab(
            PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
        assert view instanceof ViewLookupCachingFrameLayout;
        if (propertyKey == null) {
            onBindAll((ViewLookupCachingFrameLayout) view, model, TabProperties.UiType.SELECTABLE);
            return;
        }
        bindCommonProperties(model, (ViewLookupCachingFrameLayout) view, propertyKey);
        bindSelectableTabProperties(model, (ViewLookupCachingFrameLayout) view, propertyKey);
    }

    /**
     * Rebind all properties on a model to the view.
     * @param view The view to bind to.
     * @param model The model to bind.
     * @param viewType The view type to bind.
     */
    private static void onBindAll(ViewLookupCachingFrameLayout view, PropertyModel model,
            @TabProperties.UiType int viewType) {
        for (PropertyKey propertyKey : TabProperties.ALL_KEYS_TAB_GRID) {
            bindCommonProperties(model, view, propertyKey);
            switch (viewType) {
                case TabProperties.UiType.SELECTABLE:
                    bindSelectableTabProperties(model, view, propertyKey);
                    break;
                case TabProperties.UiType.CLOSABLE:
                    bindClosableTabProperties(model, view, propertyKey);
                    break;
                default:
                    assert false;
            }
        }
    }

    private static void bindCommonProperties(PropertyModel model, ViewLookupCachingFrameLayout view,
            @Nullable PropertyKey propertyKey) {
        if (TabProperties.TITLE == propertyKey) {
            String title = model.get(TabProperties.TITLE);
            ((TextView) view.fastFindViewById(R.id.tab_title)).setText(title);
        } else if (TabProperties.IS_SELECTED == propertyKey) {
            int selectedTabBackground =
                    model.get(TabProperties.SELECTED_TAB_BACKGROUND_DRAWABLE_ID);
            view.setSelected(model.get(TabProperties.IS_SELECTED));
            if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.LOLLIPOP_MR1) {
                if (model.get(TabProperties.IS_SELECTED)) {
                    view.fastFindViewById(R.id.selected_view_below_lollipop)
                            .setBackgroundResource(selectedTabBackground);
                    view.fastFindViewById(R.id.selected_view_below_lollipop)
                            .setVisibility(View.VISIBLE);
                } else {
                    view.fastFindViewById(R.id.selected_view_below_lollipop)
                            .setVisibility(View.GONE);
                }
            } else {
                Resources res = view.getResources();
                Resources.Theme theme = view.getContext().getTheme();
                Drawable drawable = new InsetDrawable(
                        ResourcesCompat.getDrawable(res, selectedTabBackground, theme),
                        (int) res.getDimension(R.dimen.tab_list_selected_inset));
                view.setForeground(model.get(TabProperties.IS_SELECTED) ? drawable : null);
            }
        } else if (TabProperties.FAVICON == propertyKey) {
            Drawable favicon = model.get(TabProperties.FAVICON);
            ImageView faviconView = (ImageView) view.fastFindViewById(R.id.tab_favicon);
            faviconView.setImageDrawable(favicon);
            int padding = favicon == null
                    ? 0
                    : (int) view.getResources().getDimension(R.dimen.tab_list_card_padding);
            faviconView.setPadding(padding, padding, padding, padding);
        } else if (TabProperties.THUMBNAIL_FETCHER == propertyKey) {
            updateThumbnail(view, model);
        }
    }

    private static void bindClosableTabProperties(
            PropertyModel model, ViewLookupCachingFrameLayout view, PropertyKey propertyKey) {
        if (TabProperties.TAB_CLOSED_LISTENER == propertyKey) {
            view.fastFindViewById(R.id.action_button).setOnClickListener(v -> {
                int tabId = model.get(TabProperties.TAB_ID);
                model.get(TabProperties.TAB_CLOSED_LISTENER).run(tabId);
            });
        } else if (TabProperties.TAB_SELECTED_LISTENER == propertyKey) {
            view.setOnClickListener(v -> {
                int tabId = model.get(TabProperties.TAB_ID);
                model.get(TabProperties.TAB_SELECTED_LISTENER).run(tabId);
            });
        } else if (TabProperties.CREATE_GROUP_LISTENER == propertyKey) {
            TabListMediator.TabActionListener listener =
                    model.get(TabProperties.CREATE_GROUP_LISTENER);
            ButtonCompat createGroupButton =
                    (ButtonCompat) view.fastFindViewById(R.id.create_group_button);
            if (listener == null) {
                createGroupButton.setVisibility(View.GONE);
                createGroupButton.setOnClickListener(null);
                return;
            }
            createGroupButton.setVisibility(View.VISIBLE);
            createGroupButton.setOnClickListener(v -> {
                int tabId = model.get(TabProperties.TAB_ID);
                listener.run(tabId);
            });
        } else if (TabProperties.ALPHA == propertyKey) {
            view.setAlpha(model.get(TabProperties.ALPHA));
        } else if (TabProperties.TITLE == propertyKey) {
            String title = model.get(TabProperties.TITLE);
            view.fastFindViewById(R.id.action_button)
                    .setContentDescription(view.getResources().getString(
                            R.string.accessibility_tabstrip_btn_close_tab, title));
        } else if (TabProperties.IPH_PROVIDER == propertyKey) {
            TabListMediator.IphProvider provider = model.get(TabProperties.IPH_PROVIDER);
            if (provider != null) provider.showIPH(view.fastFindViewById(R.id.tab_thumbnail));
        } else if (TabProperties.CARD_ANIMATION_STATUS == propertyKey) {
            boolean isSelected = model.get(TabProperties.IS_SELECTED);
            ((ClosableTabGridView) view)
                    .scaleTabGridCardView(
                            model.get(TabProperties.CARD_ANIMATION_STATUS), isSelected);
        } else if (TabProperties.IS_INCOGNITO == propertyKey) {
            updateColor(view, model.get(TabProperties.IS_INCOGNITO), TabProperties.UiType.CLOSABLE);
        }
    }

    private static void bindSelectableTabProperties(
            PropertyModel model, ViewLookupCachingFrameLayout view, PropertyKey propertyKey) {
        final int defaultLevel = view.getResources().getInteger(R.integer.list_item_level_default);
        final int selectedLevel =
                view.getResources().getInteger(R.integer.list_item_level_selected);
        final int tabId = model.get(TabProperties.TAB_ID);

        if (TabProperties.IS_SELECTED == propertyKey) {
            boolean isSelected = model.get(TabProperties.IS_SELECTED);
            ImageView actionButton = (ImageView) view.fastFindViewById(R.id.action_button);
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
        } else if (TabProperties.SELECTABLE_TAB_CLICKED_LISTENER == propertyKey) {
            view.setOnClickListener(v -> {
                model.get(TabProperties.SELECTABLE_TAB_CLICKED_LISTENER).run(tabId);
                ((SelectableTabGridView) view).onClick();
            });
            view.setOnLongClickListener(v -> {
                model.get(TabProperties.SELECTABLE_TAB_CLICKED_LISTENER).run(tabId);
                return ((SelectableTabGridView) view).onLongClick(view);
            });
        } else if (TabProperties.TITLE == propertyKey) {
            String title = model.get(TabProperties.TITLE);
            view.fastFindViewById(R.id.action_button)
                    .setContentDescription(view.getResources().getString(
                            R.string.accessibility_tabstrip_btn_close_tab, title));
        } else if (TabProperties.TAB_SELECTION_DELEGATE == propertyKey) {
            assert model.get(TabProperties.TAB_SELECTION_DELEGATE) != null;

            ((SelectableTabGridView) view)
                    .setSelectionDelegate(model.get(TabProperties.TAB_SELECTION_DELEGATE));
            ((SelectableTabGridView) view).setItem(tabId);
        } else if (TabProperties.IS_INCOGNITO == propertyKey) {
            updateColor(
                    view, model.get(TabProperties.IS_INCOGNITO), TabProperties.UiType.SELECTABLE);
        }
    }

    private static void updateThumbnail(ViewLookupCachingFrameLayout view, PropertyModel model) {
        TabListMediator.ThumbnailFetcher fetcher = model.get(TabProperties.THUMBNAIL_FETCHER);
        ImageView thumbnail = (ImageView) view.fastFindViewById(R.id.tab_thumbnail);
        if (fetcher == null) {
            // Release the thumbnail to save memory.
            thumbnail.setImageDrawable(null);
            thumbnail.setMinimumHeight(thumbnail.getWidth());
            return;
        }
        Callback<Bitmap> callback = result -> {
            if (result == null) {
                thumbnail.setImageDrawable(null);
                thumbnail.setMinimumHeight(thumbnail.getWidth());
            } else {
                thumbnail.setImageBitmap(result);
            }
        };
        fetcher.fetch(callback);
    }

    private static void updateColor(ViewLookupCachingFrameLayout rootView, boolean isIncognito,
            @TabProperties.UiType int viewType) {
        View cardView = rootView.fastFindViewById(R.id.card_view);
        View dividerView = rootView.fastFindViewById(R.id.divider_view);
        ImageView thumbnail = (ImageView) rootView.fastFindViewById(R.id.tab_thumbnail);
        ImageView actionButton = (ImageView) rootView.fastFindViewById(R.id.action_button);
        ChromeImageView backgroundView =
                (ChromeImageView) rootView.fastFindViewById(R.id.background_view);

        // ViewCompat.SetBackgroundTintList does not work here for L devices, because cardView is a
        // RelativeLayout, and in order for ViewCompat.SetBackgroundTintList to work on any L-
        // devices, the view has to implement the TintableBackgroundView interface. RelativeLayout
        // is not a TintableBackgroundView. The work around here is to set different drawable as the
        // background depends on the incognito mode.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            cardView.setBackground(TabUiColorProvider.getCardViewBackgroundDrawable(
                    cardView.getContext(), isIncognito));
        } else {
            ViewCompat.setBackgroundTintList(cardView,
                    TabUiColorProvider.getCardViewTintList(cardView.getContext(), isIncognito));
        }

        dividerView.setBackgroundColor(
                TabUiColorProvider.getDividerColor(dividerView.getContext(), isIncognito));

        ApiCompatibilityUtils.setTextAppearance(
                ((TextView) rootView.fastFindViewById(R.id.tab_title)),
                TabUiColorProvider.getTitleTextAppearance(isIncognito));

        if (thumbnail.getDrawable() == null) {
            thumbnail.setImageResource(
                    TabUiColorProvider.getThumbnailPlaceHolderColorResource(isIncognito));
        }

        if (FeatureUtilities.isTabGroupsAndroidEnabled()) {
            ViewCompat.setBackgroundTintList(backgroundView,
                    TabUiColorProvider.getHoveredCardBackgroundTintList(
                            backgroundView.getContext(), isIncognito));
        }

        if (viewType == TabProperties.UiType.CLOSABLE) {
            ApiCompatibilityUtils.setImageTintList(actionButton,
                    TabUiColorProvider.getActionButtonTintList(
                            actionButton.getContext(), isIncognito));
        }
    }
}
