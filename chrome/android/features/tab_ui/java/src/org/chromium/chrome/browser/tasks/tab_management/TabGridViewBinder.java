// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.util.Size;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tab_ui.TabUiThemeUtils;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabActionButtonData;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChromeImageView;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

/**
 * {@link org.chromium.ui.modelutil.SimpleRecyclerViewMcp.ViewBinder} for tab grid. This class
 * supports both full and partial updates to the {@link TabGridViewHolder}.
 */
class TabGridViewBinder {
    private static ThumbnailFetcher sThumbnailFetcherForTesting;
    private static final String SHOPPING_METRICS_IDENTIFIER = "EnterTabSwitcher";

    /**
     * Main entrypoint for binding TabGridView
     *
     * @param view The view to bind to.
     * @param model The model to bind.
     * @param viewType The view type to bind.
     */
    public static void bindTab(
            PropertyModel model, ViewGroup view, @Nullable PropertyKey propertyKey) {
        assert view instanceof ViewLookupCachingFrameLayout;
        @TabActionState Integer tabActionState = model.get(TabProperties.TAB_ACTION_STATE);
        if (tabActionState == null) {
            assert false : "TAB_ACTION_STATE must be set before initial bindTab call.";
            return;
        }

        ((TabGridView) view).setTabActionState(tabActionState);
        if (propertyKey == null) {
            onBindAll((ViewLookupCachingFrameLayout) view, model, tabActionState);
            return;
        }

        bindCommonProperties(model, (ViewLookupCachingFrameLayout) view, propertyKey);
        if (tabActionState == TabActionState.CLOSABLE) {
            bindClosableTabProperties(model, (ViewLookupCachingFrameLayout) view, propertyKey);
        } else if (tabActionState == TabActionState.SELECTABLE) {
            bindSelectableTabProperties(model, (ViewLookupCachingFrameLayout) view, propertyKey);
        } else {
            assert false : "Unsupported TabActionState provided to bindTab.";
        }
    }

    /**
     * Rebind all properties on a model to the view.
     *
     * @param view The view to bind to.
     * @param model The model to bind.
     * @param viewType The view type to bind.
     */
    private static void onBindAll(
            ViewLookupCachingFrameLayout view,
            PropertyModel model,
            @TabActionState int tabActionState) {
        for (PropertyKey propertyKey : TabProperties.ALL_KEYS_TAB_GRID) {
            bindCommonProperties(model, view, propertyKey);
            switch (tabActionState) {
                case TabProperties.TabActionState.SELECTABLE:
                    bindSelectableTabProperties(model, view, propertyKey);
                    break;
                case TabProperties.TabActionState.CLOSABLE:
                    bindClosableTabProperties(model, view, propertyKey);
                    break;
                default:
                    assert false;
            }
        }
    }

    private static void bindCommonProperties(
            PropertyModel model,
            ViewLookupCachingFrameLayout view,
            @Nullable PropertyKey propertyKey) {
        if (TabProperties.TITLE == propertyKey) {
            String title = model.get(TabProperties.TITLE);
            TextView tabTitleView = (TextView) view.fastFindViewById(R.id.tab_title);
            tabTitleView.setText(title);
            tabTitleView.setContentDescription(
                    view.getResources().getString(R.string.accessibility_tabstrip_tab, title));
        } else if (TabProperties.IS_SELECTED == propertyKey) {
            updateColor(
                    view,
                    model.get(TabProperties.IS_INCOGNITO),
                    model.get(TabProperties.IS_SELECTED));
            updateFavicon(view, model);
        } else if (TabProperties.FAVICON_FETCHER == propertyKey) {
            updateFavicon(view, model);
        } else if (TabProperties.CONTENT_DESCRIPTION_STRING == propertyKey) {
            view.setContentDescription(model.get(TabProperties.CONTENT_DESCRIPTION_STRING));
        } else if (TabProperties.GRID_CARD_SIZE == propertyKey) {
            final Size cardSize = model.get(TabProperties.GRID_CARD_SIZE);
            view.setMinimumHeight(cardSize.getHeight());
            view.setMinimumWidth(cardSize.getWidth());
            var layoutParams = view.getLayoutParams();
            layoutParams.height = cardSize.getHeight();
            layoutParams.width = cardSize.getWidth();
            view.setLayoutParams(layoutParams);
            updateThumbnail(view, model);
        } else if (TabProperties.THUMBNAIL_FETCHER == propertyKey) {
            updateThumbnail(view, model);
        } else if (TabProperties.TAB_ACTION_BUTTON_DATA == propertyKey) {
            @Nullable TabActionButtonData data = model.get(TabProperties.TAB_ACTION_BUTTON_DATA);
            @Nullable
            TabActionListener tabActionListener = data == null ? null : data.tabActionListener;
            setNullableClickListener(
                    tabActionListener, view.fastFindViewById(R.id.action_button), model);

            boolean showOverflowButton =
                    data == null ? false : data.type == TabActionButtonType.OVERFLOW;
            ((TabGridView) view).setTabActionButtonDrawable(showOverflowButton);
        } else if (TabProperties.TAB_CLICK_LISTENER == propertyKey) {
            setNullableClickListener(model.get(TabProperties.TAB_CLICK_LISTENER), view, model);
        } else if (TabProperties.TAB_LONG_CLICK_LISTENER == propertyKey) {
            setNullableLongClickListener(
                    model.get(TabProperties.TAB_LONG_CLICK_LISTENER), view, model);
        }
    }

    private static void bindClosableTabProperties(
            PropertyModel model, ViewLookupCachingFrameLayout view, PropertyKey propertyKey) {
        if (CARD_ALPHA == propertyKey) {
            view.setAlpha(model.get(CARD_ALPHA));
        } else if (TabProperties.CARD_ANIMATION_STATUS == propertyKey) {
            ((TabGridView) view)
                    .scaleTabGridCardView(model.get(TabProperties.CARD_ANIMATION_STATUS));
        } else if (TabProperties.ACCESSIBILITY_DELEGATE == propertyKey) {
            view.setAccessibilityDelegate(model.get(TabProperties.ACCESSIBILITY_DELEGATE));
        } else if (TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER == propertyKey) {
            fetchPriceDrop(model, (priceDrop) -> onPriceDropFetched(view, model, priceDrop), true);
        } else if (TabProperties.SHOULD_SHOW_PRICE_DROP_TOOLTIP == propertyKey) {
            if (model.get(TabProperties.SHOULD_SHOW_PRICE_DROP_TOOLTIP)) {
                PriceCardView priceCardView =
                        (PriceCardView) view.fastFindViewById(R.id.price_info_box_outer);
                assert priceCardView.getVisibility() == View.VISIBLE;
                LargeMessageCardView.showPriceDropTooltip(
                        priceCardView.findViewById(R.id.current_price));
            }
        } else if (TabProperties.ACTION_BUTTON_DESCRIPTION_STRING == propertyKey) {
            view.fastFindViewById(R.id.action_button)
                    .setContentDescription(
                            model.get(TabProperties.ACTION_BUTTON_DESCRIPTION_STRING));
        } else if (TabProperties.QUICK_DELETE_ANIMATION_STATUS == propertyKey) {
            ((TabGridView) view)
                    .hideTabGridCardViewForQuickDelete(
                            model.get(TabProperties.QUICK_DELETE_ANIMATION_STATUS),
                            model.get(TabProperties.IS_INCOGNITO));
        } else if (TabProperties.VISIBILITY == propertyKey) {
            view.setVisibility(model.get(TabProperties.VISIBILITY));
        } else if (TabProperties.IS_SELECTED == propertyKey
                || TabProperties.TAB_ACTION_BUTTON_DATA == propertyKey) {
            updateColorForActionButton(
                    view,
                    model.get(TabProperties.IS_INCOGNITO),
                    model.get(TabProperties.IS_SELECTED));
        } else if (TabProperties.TAB_CARD_LABEL_DATA == propertyKey) {
            updateTabCardLabel(view, model.get(TabProperties.TAB_CARD_LABEL_DATA));
        }
    }

    private static void bindSelectableTabProperties(
            PropertyModel model, ViewLookupCachingFrameLayout view, PropertyKey propertyKey) {
        final int tabId = model.get(TabProperties.TAB_ID);

        if (TabProperties.TAB_SELECTION_DELEGATE == propertyKey) {
            ((TabGridView) view)
                    .setSelectionDelegate(model.get(TabProperties.TAB_SELECTION_DELEGATE));
            ((TabGridView) view).setItem(tabId);
        } else if (TabProperties.IS_SELECTED == propertyKey
                || TabProperties.TAB_ACTION_BUTTON_DATA == propertyKey) {
            updateColorForSelectionToggleButton(
                    view,
                    model.get(TabProperties.IS_INCOGNITO),
                    model.get(TabProperties.IS_SELECTED));
        } else if (TabProperties.TAB_CARD_LABEL_DATA == propertyKey) {
            // Ignore this data for tab card labels in selectable mode.
            updateTabCardLabel(view, /* tabCardLabelData= */ null);
        }
    }

    static void setNullableClickListener(
            @Nullable TabActionListener listener,
            @NonNull View view,
            @NonNull PropertyModel propertyModel) {
        if (listener == null) {
            view.setOnClickListener(null);
        } else {
            view.setOnClickListener(
                    v -> {
                        listener.run(v, propertyModel.get(TabProperties.TAB_ID));
                    });
        }
    }

    static void setNullableLongClickListener(
            @Nullable TabActionListener listener,
            @NonNull View view,
            @NonNull PropertyModel propertyModel) {
        if (listener == null) {
            view.setOnLongClickListener(null);
        } else {
            view.setOnLongClickListener(
                    v -> {
                        listener.run(v, propertyModel.get(TabProperties.TAB_ID));
                        return true;
                    });
        }
    }

    private static void fetchPriceDrop(
            PropertyModel model,
            Callback<ShoppingPersistedTabData.PriceDrop> callback,
            boolean shouldLog) {
        if (model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER) == null) {
            callback.onResult(null);
            return;
        }
        model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER)
                .fetch(
                        (shoppingPersistedTabData) -> {
                            if (shoppingPersistedTabData == null) {
                                callback.onResult(null);
                                return;
                            }
                            if (shouldLog) {
                                shoppingPersistedTabData.logPriceDropMetrics(
                                        SHOPPING_METRICS_IDENTIFIER);
                            }
                            callback.onResult(shoppingPersistedTabData.getPriceDrop());
                        });
    }

    private static void onPriceDropFetched(
            ViewLookupCachingFrameLayout rootView,
            PropertyModel model,
            @Nullable ShoppingPersistedTabData.PriceDrop priceDrop) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING)) {
            // TODO(crbug.com/361169665): Do activity updates or price drops take priority. Assume
            // activity updates win for now.
            if (model.get(TabProperties.TAB_CARD_LABEL_DATA) != null) return;

            if (priceDrop == null) {
                updateTabCardLabel(rootView, null);
                return;
            }

            TextResolver contentDescriptionResolver =
                    (context) -> {
                        return context.getResources()
                                .getString(
                                        R.string.accessibility_tab_price_card,
                                        priceDrop.previousPrice,
                                        priceDrop.price);
                    };
            PriceDropTextResolver priceDropResolver =
                    new PriceDropTextResolver(priceDrop.price, priceDrop.previousPrice);
            TabCardLabelData labelData =
                    new TabCardLabelData(
                            TabCardLabelType.PRICE_DROP,
                            priceDropResolver,
                            /* asyncImageFactory= */ null,
                            contentDescriptionResolver);
            updateTabCardLabel(rootView, labelData);
        } else {
            PriceCardView priceCardView =
                    (PriceCardView) rootView.fastFindViewById(R.id.price_info_box_outer);
            if (priceDrop == null) {
                priceCardView.setVisibility(View.GONE);
                return;
            }
            priceCardView.setPriceStrings(priceDrop.price, priceDrop.previousPrice);
            priceCardView.setVisibility(View.VISIBLE);
            priceCardView.setContentDescription(
                    rootView.getResources()
                            .getString(
                                    R.string.accessibility_tab_price_card,
                                    priceDrop.previousPrice,
                                    priceDrop.price));
        }
    }

    private static void updateThumbnail(ViewLookupCachingFrameLayout view, PropertyModel model) {
        TabThumbnailView thumbnail = (TabThumbnailView) view.fastFindViewById(R.id.tab_thumbnail);

        // To GC on hide set a background color and remove the thumbnail.
        final boolean isSelected = model.get(TabProperties.IS_SELECTED);
        thumbnail.updateThumbnailPlaceholder(model.get(TabProperties.IS_INCOGNITO), isSelected);

        final ThumbnailFetcher fetcher = model.get(TabProperties.THUMBNAIL_FETCHER);
        final Size cardSize = model.get(TabProperties.GRID_CARD_SIZE);
        if (fetcher == null || cardSize == null) {
            thumbnail.setImageDrawable(null);
            return;
        }

        // TODO(crbug.com/40882123): Consider unsetting the bitmap early to allow memory reuse if
        // needed.
        final Size thumbnailSize = TabUtils.deriveThumbnailSize(cardSize, view.getContext());
        // This callback will be made cancelable inside ThumbnailFetcher so only the latest fetch
        // request will return. When the fetcher is replaced any outbound requests are first
        // canceled inside TabListMediator so it is not necessary to do any sort of validation that
        // the callback matches the current thumbnail fetcher and grid card size.
        Callback<Drawable> callback =
                result -> {
                    if (result != null) {
                        TabUtils.setDrawableAndUpdateImageMatrix(thumbnail, result, thumbnailSize);
                    } else {
                        thumbnail.setImageDrawable(null);
                    }
                };
        if (sThumbnailFetcherForTesting != null) {
            sThumbnailFetcherForTesting.fetch(thumbnailSize, isSelected, callback);
        } else {
            fetcher.fetch(thumbnailSize, isSelected, callback);
        }
    }

    /**
     * Update the favicon drawable to use from {@link TabListFaviconProvider.TabFavicon}, and the
     * padding around it. The color work is already handled when favicon is bind in {@link
     * #bindCommonProperties}.
     */
    private static void updateFavicon(ViewLookupCachingFrameLayout rootView, PropertyModel model) {
        final TabListFaviconProvider.TabFaviconFetcher fetcher =
                model.get(TabProperties.FAVICON_FETCHER);
        if (fetcher == null) {
            setFavicon(rootView, model, null);
            return;
        }
        fetcher.fetch(
                tabFavicon -> {
                    if (fetcher != model.get(TabProperties.FAVICON_FETCHER)) return;

                    setFavicon(rootView, model, tabFavicon);
                });
    }

    /**
     * Set the favicon drawable to use from {@link TabListFaviconProvider.TabFavicon}, and the
     * padding around it. The color work is already handled when favicon is bind in {@link
     * #bindCommonProperties}.
     */
    private static void setFavicon(
            ViewLookupCachingFrameLayout rootView,
            PropertyModel model,
            TabListFaviconProvider.TabFavicon favicon) {
        ImageView faviconView = (ImageView) rootView.fastFindViewById(R.id.tab_favicon);
        if (favicon == null) {
            faviconView.setImageDrawable(null);
            faviconView.setPadding(0, 0, 0, 0);
            return;
        }

        boolean isSelected = model.get(TabProperties.IS_SELECTED);
        faviconView.setImageDrawable(
                isSelected ? favicon.getSelectedDrawable() : favicon.getDefaultDrawable());
        int padding =
                (int) TabUiThemeProvider.getTabCardTopFaviconPadding(faviconView.getContext());
        faviconView.setPadding(padding, padding, padding, padding);
    }

    private static void updateColor(
            ViewLookupCachingFrameLayout rootView, boolean isIncognito, boolean isSelected) {
        View cardView = rootView.fastFindViewById(R.id.card_view);
        TextView titleView = (TextView) rootView.fastFindViewById(R.id.tab_title);
        TabThumbnailView thumbnail =
                (TabThumbnailView) rootView.fastFindViewById(R.id.tab_thumbnail);
        ChromeImageView backgroundView =
                (ChromeImageView) rootView.fastFindViewById(R.id.background_view);

        cardView.getBackground().mutate();
        final @ColorInt int backgroundColor =
                TabUiThemeUtils.getCardViewBackgroundColor(
                        cardView.getContext(), isIncognito, isSelected);
        ViewCompat.setBackgroundTintList(cardView, ColorStateList.valueOf(backgroundColor));

        titleView.setTextColor(
                TabUiThemeUtils.getTitleTextColor(titleView.getContext(), isIncognito, isSelected));

        thumbnail.updateThumbnailPlaceholder(isIncognito, isSelected);

        ViewCompat.setBackgroundTintList(
                backgroundView,
                TabUiThemeProvider.getHoveredCardBackgroundTintList(
                        backgroundView.getContext(), isIncognito, isSelected));
    }

    private static void updateColorForActionButton(
            ViewLookupCachingFrameLayout rootView, boolean isIncognito, boolean isSelected) {
        ImageView actionButton = (ImageView) rootView.fastFindViewById(R.id.action_button);
        ImageViewCompat.setImageTintList(
                actionButton,
                TabUiThemeProvider.getActionButtonTintList(
                        actionButton.getContext(), isIncognito, isSelected));
    }

    private static void updateColorForSelectionToggleButton(
            ViewLookupCachingFrameLayout rootView, boolean isIncognito, boolean isSelected) {
        ImageView actionButton = (ImageView) rootView.fastFindViewById(R.id.action_button);

        Context context = rootView.getContext();
        Resources res = rootView.getResources();
        actionButton.getBackground().setLevel(getCheckmarkLevel(res, isSelected));
        DrawableCompat.setTintList(
                actionButton.getBackground().mutate(),
                TabUiThemeProvider.getToggleActionButtonBackgroundTintList(
                        context, isIncognito, isSelected));

        // The check should be invisible if not selected.
        Drawable drawable = actionButton.getDrawable();
        drawable.setAlpha(isSelected ? 255 : 0);
        ImageViewCompat.setImageTintList(
                actionButton,
                isSelected
                        ? TabUiThemeProvider.getToggleActionButtonCheckedDrawableTintList(
                                context, isIncognito)
                        : null);

        if (isSelected) {
            ((AnimatedVectorDrawableCompat) drawable).start();
        }
    }

    private static void updateTabCardLabel(
            ViewLookupCachingFrameLayout rootView, @Nullable TabCardLabelData tabCardLabelData) {
        @Nullable ViewStub stub = (ViewStub) rootView.fastFindViewById(R.id.tab_card_label_stub);
        TabCardLabelView labelView;
        if (stub != null) {
            if (tabCardLabelData == null) return;

            labelView = (TabCardLabelView) stub.inflate();
        } else {
            labelView = (TabCardLabelView) rootView.fastFindViewById(R.id.tab_card_label);
        }
        labelView.setData(tabCardLabelData);
    }

    private static int getCheckmarkLevel(Resources res, boolean isSelected) {
        return isSelected
                ? res.getInteger(R.integer.list_item_level_selected)
                : res.getInteger(R.integer.list_item_level_default);
    }

    static void setThumbnailFeatureForTesting(ThumbnailFetcher fetcher) {
        sThumbnailFetcherForTesting = fetcher;
        ResettersForTesting.register(() -> sThumbnailFetcherForTesting = null);
    }
}
