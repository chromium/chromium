// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.os.Build;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.res.ResourcesCompat;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.core.view.ViewCompat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChipView;
import org.chromium.ui.widget.ChromeImageView;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

/**
 * {@link org.chromium.ui.modelutil.SimpleRecyclerViewMcp.ViewBinder} for tab grid.
 * This class supports both full and partial updates to the {@link TabGridViewHolder}.
 */
class TabGridViewBinder {
    private static TabListMediator.ThumbnailFetcher sThumbnailFetcherForTesting;
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
            TextView tabTitleView = (TextView) view.fastFindViewById(R.id.tab_title);
            tabTitleView.setText(title);
            tabTitleView.setContentDescription(
                    view.getResources().getString(R.string.accessibility_tabstrip_tab, title));
        } else if (TabProperties.IS_SELECTED == propertyKey) {
            int selectedTabBackground =
                    model.get(TabProperties.SELECTED_TAB_BACKGROUND_DRAWABLE_ID);
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
            if (TabUiFeatureUtilities.ENABLE_SEARCH_CHIP.getValue()) {
                ChipView pageInfoButton = (ChipView) view.fastFindViewById(R.id.page_info_button);
                pageInfoButton.getPrimaryTextView().setTextAlignment(
                        View.TEXT_ALIGNMENT_VIEW_START);
                pageInfoButton.getPrimaryTextView().setEllipsize(TextUtils.TruncateAt.END);
                // TODO(crbug.com/1048255): The selected state of ChipView doesn't look elevated.
                //  Fix the elevation in style instead.
                pageInfoButton.setSelected(false);
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
        } else if (TabProperties.CONTENT_DESCRIPTION_STRING == propertyKey) {
            view.setContentDescription(model.get(TabProperties.CONTENT_DESCRIPTION_STRING));
        }
    }

    private static void bindClosableTabProperties(
            PropertyModel model, ViewLookupCachingFrameLayout view, PropertyKey propertyKey) {
        if (TabProperties.TAB_CLOSED_LISTENER == propertyKey) {
            if (model.get(TabProperties.TAB_CLOSED_LISTENER) == null) {
                view.fastFindViewById(R.id.action_button).setOnClickListener(null);
            } else {
                view.fastFindViewById(R.id.action_button).setOnClickListener(v -> {
                    int tabId = model.get(TabProperties.TAB_ID);
                    model.get(TabProperties.TAB_CLOSED_LISTENER).run(tabId);
                });
            }
        } else if (TabProperties.TAB_SELECTED_LISTENER == propertyKey) {
            if (model.get(TabProperties.TAB_SELECTED_LISTENER) == null) {
                view.setOnClickListener(null);
            } else {
                view.setOnClickListener(v -> {
                    int tabId = model.get(TabProperties.TAB_ID);
                    model.get(TabProperties.TAB_SELECTED_LISTENER).run(tabId);
                });
            }
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
        } else if (CARD_ALPHA == propertyKey) {
            view.setAlpha(model.get(CARD_ALPHA));
        } else if (TabProperties.TITLE == propertyKey) {
            if (TabUiFeatureUtilities.isLaunchPolishEnabled()) return;
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
        } else if (TabProperties.ACCESSIBILITY_DELEGATE == propertyKey) {
            view.setAccessibilityDelegate(model.get(TabProperties.ACCESSIBILITY_DELEGATE));
        } else if (TabProperties.SEARCH_QUERY == propertyKey) {
            String query = model.get(TabProperties.SEARCH_QUERY);
            ChipView pageInfoButton = (ChipView) view.fastFindViewById(R.id.page_info_button);
            if (TextUtils.isEmpty(query)) {
                pageInfoButton.setVisibility(View.GONE);
            } else {
                // Search query and price string are mutually exclusive
                assert model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER) == null;
                pageInfoButton.setVisibility(View.VISIBLE);
                pageInfoButton.getPrimaryTextView().setText(query);
            }
        } else if (TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER == propertyKey) {
            PriceCardView priceCardView =
                    (PriceCardView) view.fastFindViewById(R.id.price_info_box_outer);
            if (model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER) != null) {
                model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER)
                        .fetch((shoppingPersistedTabData) -> {
                            if (shoppingPersistedTabData == null
                                    || shoppingPersistedTabData.getPriceDrop() == null) {
                                priceCardView.setVisibility(View.GONE);
                            } else {
                                priceCardView.setPriceStrings(
                                        shoppingPersistedTabData.getPriceDrop().price,
                                        shoppingPersistedTabData.getPriceDrop().previousPrice);
                                priceCardView.setVisibility(View.VISIBLE);
                            }
                        });
            } else {
                priceCardView.setVisibility(View.GONE);
            }
        } else if (TabProperties.SHOULD_SHOW_PRICE_DROP_TOOLTIP == propertyKey) {
            if (model.get(TabProperties.SHOULD_SHOW_PRICE_DROP_TOOLTIP)) {
                PriceCardView priceCardView =
                        (PriceCardView) view.fastFindViewById(R.id.price_info_box_outer);
                assert priceCardView.getVisibility() == View.VISIBLE;
                LargeMessageCardView.showPriceDropTooltip(
                        priceCardView.findViewById(R.id.current_price));
            }
        } else if (TabProperties.PAGE_INFO_LISTENER == propertyKey) {
            TabListMediator.TabActionListener listener =
                    model.get(TabProperties.PAGE_INFO_LISTENER);
            ChipView pageInfoButton = (ChipView) view.fastFindViewById(R.id.page_info_button);
            if (listener == null) {
                pageInfoButton.setOnClickListener(null);
                return;
            }
            pageInfoButton.setOnClickListener(v -> {
                int tabId = model.get(TabProperties.TAB_ID);
                listener.run(tabId);
            });
        } else if (TabProperties.PAGE_INFO_ICON_DRAWABLE_ID == propertyKey) {
            ChipView pageInfoButton = (ChipView) view.fastFindViewById(R.id.page_info_button);
            int iconDrawableId = model.get(TabProperties.PAGE_INFO_ICON_DRAWABLE_ID);
            boolean shouldTint = iconDrawableId != R.drawable.ic_logo_googleg_24dp;
            pageInfoButton.setIcon(iconDrawableId, shouldTint);
        } else if (TabProperties.IS_SELECTED == propertyKey) {
            view.setSelected(model.get(TabProperties.IS_SELECTED));
        } else if (TabUiFeatureUtilities.isLaunchPolishEnabled()
                && TabProperties.CLOSE_BUTTON_DESCRIPTION_STRING == propertyKey) {
            view.fastFindViewById(R.id.action_button)
                    .setContentDescription(
                            model.get(TabProperties.CLOSE_BUTTON_DESCRIPTION_STRING));
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
            if (isSelected) {
                ((AnimatedVectorDrawableCompat) actionButton.getDrawable()).start();
            }
        } else if (TabProperties.SELECTABLE_TAB_CLICKED_LISTENER == propertyKey) {
            view.setOnClickListener(v -> {
                model.get(TabProperties.SELECTABLE_TAB_CLICKED_LISTENER).run(tabId);
                ((SelectableTabGridView) view).onClick();
            });
            view.setOnLongClickListener(v -> {
                model.get(TabProperties.SELECTABLE_TAB_CLICKED_LISTENER).run(tabId);
                return ((SelectableTabGridView) view).onLongClick(view);
            });
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
            releaseThumbnail(thumbnail);
            return;
        }
        Callback<Bitmap> callback = result -> {
            if (result == null) {
                releaseThumbnail(thumbnail);
            } else {
                thumbnail.setImageBitmap(result);
            }
        };
        if (TabUiFeatureUtilities.isLaunchPolishEnabled() && sThumbnailFetcherForTesting != null) {
            sThumbnailFetcherForTesting.fetch(callback);
        } else {
            fetcher.fetch(callback);
        }
    }

    private static void releaseThumbnail(ImageView thumbnail) {
        if (TabUiFeatureUtilities.isLaunchPolishEnabled()) {
            thumbnail.setImageDrawable(null);
            return;
        }

        if (TabUiFeatureUtilities.isTabThumbnailAspectRatioNotOne()) {
            float expectedThumbnailAspectRatio =
                    (float) TabUiFeatureUtilities.THUMBNAIL_ASPECT_RATIO.getValue();
            expectedThumbnailAspectRatio =
                    MathUtils.clamp(expectedThumbnailAspectRatio, 0.5f, 2.0f);
            int height = (int) (thumbnail.getWidth() * 1.0 / expectedThumbnailAspectRatio);
            thumbnail.setMinimumHeight(Math.min(thumbnail.getHeight(), height));
            thumbnail.setImageDrawable(null);
        } else {
            thumbnail.setImageDrawable(null);
            thumbnail.setMinimumHeight(thumbnail.getWidth());
        }
    }

    private static void updateColor(ViewLookupCachingFrameLayout rootView, boolean isIncognito,
            @TabProperties.UiType int viewType) {
        View cardView = rootView.fastFindViewById(R.id.card_view);
        View dividerView = rootView.fastFindViewById(R.id.divider_view);
        ImageView thumbnail = (ImageView) rootView.fastFindViewById(R.id.tab_thumbnail);
        ImageView actionButton = (ImageView) rootView.fastFindViewById(R.id.action_button);
        ChromeImageView backgroundView =
                (ChromeImageView) rootView.fastFindViewById(R.id.background_view);

        ViewCompat.setBackgroundTintList(cardView,
                TabUiColorProvider.getCardViewTintList(cardView.getContext(), isIncognito));

        dividerView.setBackgroundColor(
                TabUiColorProvider.getDividerColor(dividerView.getContext(), isIncognito));

        ApiCompatibilityUtils.setTextAppearance(
                ((TextView) rootView.fastFindViewById(R.id.tab_title)),
                TabUiColorProvider.getTitleTextAppearance(isIncognito));

        if (thumbnail.getDrawable() == null) {
            thumbnail.setImageResource(
                    TabUiColorProvider.getThumbnailPlaceHolderColorResource(isIncognito));
        }

        if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled()) {
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

    @VisibleForTesting
    static void setThumbnailFeatureForTesting(TabListMediator.ThumbnailFetcher fetcher) {
        sThumbnailFetcherForTesting = fetcher;
    }
}
