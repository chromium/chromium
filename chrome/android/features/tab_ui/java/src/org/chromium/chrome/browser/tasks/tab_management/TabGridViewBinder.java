// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.View.GONE;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.util.Size;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.StringRes;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.core.view.ViewCompat;
import androidx.core.widget.ImageViewCompat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab.MediaState;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData.PriceDrop;
import org.chromium.chrome.browser.tab_ui.TabCardThemeUtil;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tab_ui.TabThumbnailView;
import org.chromium.chrome.browser.tasks.tab_management.TabActionButtonData.TabActionButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.ShoppingPersistedTabDataFetcher;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabActionState;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.TabCardHighlightState;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.util.motion.OnPeripheralClickListener;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ChromeImageView;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

/**
 * {@link org.chromium.ui.modelutil.SimpleRecyclerViewMcp.ViewBinder} for tab grid. This class
 * supports both full and partial updates to the {@link TabGridViewHolder}.
 */
@NullMarked
class TabGridViewBinder {
    private static @Nullable ThumbnailFetcher sThumbnailFetcherForTesting;
    private static final String SHOPPING_METRICS_IDENTIFIER = "EnterTabSwitcher";

    /**
     * Main entrypoint for binding TabGridView
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
     * Handles any cleanup for recycled views that might be expensive to keep around in the pool.
     *
     * @param model The property model to possibly cleanup.
     * @param view The view to possibly cleanup.
     */
    public static void onViewRecycled(PropertyModel model, View view) {
        if (view instanceof TabGridView tabGridView) {
            TabThumbnailView thumbnail = tabGridView.fastFindViewById(R.id.tab_thumbnail);
            thumbnail.setImageDrawable(null);

            ThumbnailFetcher fetcher = model.get(TabProperties.THUMBNAIL_FETCHER);
            if (fetcher != null) fetcher.cancel();

            ImageView faviconView = tabGridView.fastFindViewById(R.id.tab_favicon);
            setFavicon(faviconView, model, /* favicon= */ null);

            // Ensure the tab group color view can be attached to a new parent if it exists.
            FrameLayout container =
                    tabGridView.fastFindViewById(R.id.tab_group_color_view_container);
            TabCardViewBinderUtils.detachTabGroupColorView(container);

            tabGridView.clearHighlight();
        }
    }

    /**
     * Rebind all properties on a model to the view.
     *
     * @param view The view to bind to.
     * @param model The model to bind.
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
        if (TabProperties.TITLE == propertyKey
                || TabProperties.IS_PINNED == propertyKey
                || TabProperties.MEDIA_INDICATOR == propertyKey) {
            String title = model.get(TabProperties.TITLE);
            TextView tabTitleView = view.fastFindViewById(R.id.tab_title);
            if (TabProperties.TITLE == propertyKey) tabTitleView.setText(title);
            if (TabProperties.MEDIA_INDICATOR == propertyKey) {
                ((TabGridView) view).setMediaIndicator(model.get(TabProperties.MEDIA_INDICATOR));
            }
            boolean isPinned =
                    model.containsKey(TabProperties.IS_PINNED)
                            && model.get(TabProperties.IS_PINNED);
            @MediaState
            int mediaState =
                    model.containsKey(TabProperties.MEDIA_INDICATOR)
                            ? model.get(TabProperties.MEDIA_INDICATOR)
                            : MediaState.NONE;
            @StringRes
            int contentDescriptionStringId = getTabContentDescriptionStringId(isPinned, mediaState);
            tabTitleView.setContentDescription(
                    view.getResources().getString(contentDescriptionStringId, title));
        } else if (TabProperties.IS_SELECTED == propertyKey) {
            updateColor(
                    view,
                    model.get(TabProperties.IS_INCOGNITO),
                    model.get(TabProperties.IS_SELECTED),
                    model.get(TabProperties.TAB_GROUP_CARD_COLOR));
            updateFavicon(view, model);
        } else if (TabProperties.TAB_GROUP_CARD_COLOR == propertyKey) {
            updateColor(
                    view,
                    model.get(TabProperties.IS_INCOGNITO),
                    model.get(TabProperties.IS_SELECTED),
                    model.get(TabProperties.TAB_GROUP_CARD_COLOR));
            updateFavicon(view, model);
        } else if (TabProperties.FAVICON_FETCHER == propertyKey) {
            updateFavicon(view, model);
        } else if (TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER == propertyKey) {
            @Nullable
            TabGroupColorViewProvider provider =
                    model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER);
            FrameLayout container = view.fastFindViewById(R.id.tab_group_color_view_container);
            TabCardViewBinderUtils.updateTabGroupColorView(container, provider);
        } else if (TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER == propertyKey) {
            TextResolver contentDescriptionTextResolver =
                    model.get(TabProperties.CONTENT_DESCRIPTION_TEXT_RESOLVER);
            CharSequence contentDescriptionString =
                    TabCardViewBinderUtils.resolveNullSafe(
                            contentDescriptionTextResolver, view.getContext());
            view.setContentDescription(contentDescriptionString);
        } else if (TabProperties.GRID_CARD_SIZE == propertyKey) {
            final Size cardSize = model.get(TabProperties.GRID_CARD_SIZE);
            int height = cardSize.getHeight();
            int width = cardSize.getWidth();
            var layoutParams = view.getLayoutParams();
            boolean sizeChanged =
                    view.getMinimumHeight() != height
                            || view.getMinimumWidth() != width
                            || layoutParams.height != height
                            || layoutParams.width != width;
            if (sizeChanged) {
                // Only update if the size changed to avoid needless layout requests which are
                // expensive. A noop is likely to happen if the view gets recycled and re-bound as
                // all the tab cards have the same size.
                view.setMinimumHeight(height);
                view.setMinimumWidth(width);
                layoutParams.height = height;
                layoutParams.width = width;
                view.setLayoutParams(layoutParams);
            }
            // If the size changed we always need to update. Otherwise we only need to update if the
            // current thumbnail is a placeholder and we were waiting on a thumbnail.
            updateThumbnail(view, model, /* onlyUpdateIfPlaceholder= */ !sizeChanged);
        } else if (TabProperties.THUMBNAIL_FETCHER == propertyKey) {
            updateThumbnail(view, model, /* onlyUpdateIfPlaceholder= */ false);
        } else if (TabProperties.TAB_ACTION_BUTTON_DATA == propertyKey) {
            @Nullable TabActionButtonData data = model.get(TabProperties.TAB_ACTION_BUTTON_DATA);
            @Nullable
            TabActionListener tabActionListener = data == null ? null : data.tabActionListener;
            setNullableClickListener(
                    tabActionListener, view.fastFindViewById(R.id.action_button), model);
            setNullablePeripheralClickListener(
                    tabActionListener, view.fastFindViewById(R.id.action_button), model);

            @TabActionButtonType
            int actionButtonType = data != null ? data.type : TabActionButtonType.OVERFLOW;
            ((TabGridView) view).setTabActionButtonDrawable(actionButtonType);
        } else if (TabProperties.TAB_CLICK_LISTENER == propertyKey) {
            setNullableClickListener(model.get(TabProperties.TAB_CLICK_LISTENER), view, model);
        } else if (TabProperties.TAB_LONG_CLICK_LISTENER == propertyKey) {
            setNullableLongClickListener(
                    model.get(TabProperties.TAB_LONG_CLICK_LISTENER), view, model);
        } else if (TabProperties.TAB_CONTEXT_CLICK_LISTENER == propertyKey) {
            setNullableContextClickListener(
                    model.get(TabProperties.TAB_CONTEXT_CLICK_LISTENER), view, model);
        }
    }

    private static void bindClosableTabProperties(
            PropertyModel model, ViewLookupCachingFrameLayout view, PropertyKey propertyKey) {
        if (CARD_ALPHA == propertyKey) {
            view.setAlpha(model.get(CARD_ALPHA));
        } else if (CardProperties.CARD_ANIMATION_STATUS == propertyKey) {
            ((TabGridView) view)
                    .scaleTabGridCardView(model.get(CardProperties.CARD_ANIMATION_STATUS));
        } else if (TabProperties.ACCESSIBILITY_DELEGATE == propertyKey) {
            view.setAccessibilityDelegate(model.get(TabProperties.ACCESSIBILITY_DELEGATE));
        } else if (TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER == propertyKey) {
            fetchPriceDrop(model, (priceDrop) -> onPriceDropFetched(view, model, priceDrop), true);
        } else if (TabProperties.SHOULD_SHOW_PRICE_DROP_TOOLTIP == propertyKey) {
            if (model.get(TabProperties.SHOULD_SHOW_PRICE_DROP_TOOLTIP)) {
                PriceCardView priceCardView = view.fastFindViewById(R.id.price_info_box_outer);
                assert priceCardView.getVisibility() == View.VISIBLE;
                LargeMessageCardView.showPriceDropTooltip(
                        priceCardView.findViewById(R.id.current_price));
            }
        } else if (TabProperties.ACTION_BUTTON_DESCRIPTION_TEXT_RESOLVER == propertyKey) {
            TextResolver actionButtonDescriptionTextResolver =
                    model.get(TabProperties.ACTION_BUTTON_DESCRIPTION_TEXT_RESOLVER);
            CharSequence actionButtonDescriptionString =
                    actionButtonDescriptionTextResolver == null
                            ? null
                            : actionButtonDescriptionTextResolver.resolve(view.getContext());
            view.fastFindViewById(R.id.action_button)
                    .setContentDescription(actionButtonDescriptionString);
        } else if (TabProperties.QUICK_DELETE_ANIMATION_STATUS == propertyKey) {
            ((TabGridView) view)
                    .hideTabGridCardViewForQuickDelete(
                            model.get(TabProperties.QUICK_DELETE_ANIMATION_STATUS),
                            model.get(TabProperties.IS_INCOGNITO));
        } else if (TabProperties.VISIBILITY == propertyKey) {
            view.setVisibility(model.get(TabProperties.VISIBILITY));
        } else if (TabProperties.IS_SELECTED == propertyKey
                || TabProperties.TAB_ACTION_BUTTON_DATA == propertyKey
                || TabProperties.TAB_GROUP_CARD_COLOR == propertyKey) {
            ((TabGridView) view)
                    .setTabActionButtonTint(
                            TabCardThemeUtil.getActionButtonTintList(
                                    view.getContext(),
                                    model.get(TabProperties.IS_INCOGNITO),
                                    model.get(TabProperties.IS_SELECTED),
                                    model.get(TabProperties.TAB_GROUP_CARD_COLOR)));
        } else if (TabProperties.TAB_CARD_LABEL_DATA == propertyKey) {
            updateTabCardLabel(view, model.get(TabProperties.TAB_CARD_LABEL_DATA));
        } else if (TabProperties.HIGHLIGHT_STATE == propertyKey) {
            @TabCardHighlightState int highlightState = model.get(TabProperties.HIGHLIGHT_STATE);
            ((TabGridView) view)
                    .setIsHighlighted(highlightState, model.get(TabProperties.IS_INCOGNITO));
            if (model.get(TabProperties.HIGHLIGHT_STATE)
                    == TabCardHighlightState.TO_BE_HIGHLIGHTED) {
                model.set(TabProperties.HIGHLIGHT_STATE, TabCardHighlightState.HIGHLIGHTED);
            }
        }
    }

    private static void bindSelectableTabProperties(
            PropertyModel model, ViewLookupCachingFrameLayout view, PropertyKey propertyKey) {
        if (TabProperties.TAB_SELECTION_DELEGATE == propertyKey) {
            TabListEditorItemSelectionId itemId;
            if (model.containsKey(TabProperties.TAB_GROUP_SYNC_ID)) {
                String syncId = model.get(TabProperties.TAB_GROUP_SYNC_ID);
                itemId = TabListEditorItemSelectionId.createTabGroupSyncId(syncId);
            } else {
                int tabId = model.get(TabProperties.TAB_ID);
                itemId = TabListEditorItemSelectionId.createTabId(tabId);
            }

            ((TabGridView) view)
                    .setSelectionDelegate(model.get(TabProperties.TAB_SELECTION_DELEGATE));
            ((TabGridView) view).setItem(itemId);
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
            @Nullable TabActionListener listener, View view, PropertyModel propertyModel) {
        if (listener == null) {
            view.setOnClickListener(null);
        } else {
            view.setOnClickListener(
                    v ->
                            runTabActionListener(
                                    listener, v, propertyModel, /* triggeringMotion= */ null));
        }
    }

    /**
     * Sets an {@link OnPeripheralClickListener} on the given view to intercept clicks from
     * peripherals.
     *
     * <p>Note that this method cannot replace {@link #setNullableClickListener(TabActionListener,
     * View, PropertyModel)} as an {@link android.view.View.OnClickListener} is needed to handle
     * clicks not from peripherals, accessibility actions, and the "confirm" key event.
     *
     * @param tabActionListener the {@link TabActionListener} to run when a click is detected.
     * @param view the View to receive clicks.
     * @param propertyModel contains data to determine how to run the {@link TabActionListener}.
     */
    static void setNullablePeripheralClickListener(
            @Nullable TabActionListener tabActionListener, View view, PropertyModel propertyModel) {
        if (tabActionListener == null) {
            view.setOnTouchListener(null);
            return;
        }

        view.setOnTouchListener(
                new OnPeripheralClickListener(
                        view,
                        triggeringMotion ->
                                runTabActionListener(
                                        tabActionListener, view, propertyModel, triggeringMotion)));
    }

    static void setNullableLongClickListener(
            @Nullable TabActionListener listener, View view, PropertyModel propertyModel) {
        if (listener == null) {
            view.setOnLongClickListener(null);
        } else {
            view.setOnLongClickListener(
                    v -> {
                        runTabActionListener(
                                listener, v, propertyModel, /* triggeringMotion= */ null);
                        return true;
                    });
        }
    }

    static void setNullableContextClickListener(
            @Nullable TabActionListener listener, View view, PropertyModel propertyModel) {
        if (listener == null) {
            view.setContextClickable(false);
            view.setOnContextClickListener(null);
        } else {
            view.setContextClickable(true);
            view.setOnContextClickListener(
                    v -> {
                        runTabActionListener(
                                listener, v, propertyModel, /* triggeringMotion= */ null);
                        return true;
                    });
        }
    }

    private static void runTabActionListener(
            TabActionListener tabActionListener,
            View view,
            PropertyModel propertyModel,
            @Nullable MotionEventInfo triggeringMotion) {
        if (propertyModel.containsKey(TabProperties.TAB_GROUP_SYNC_ID)) {
            tabActionListener.run(
                    view, propertyModel.get(TabProperties.TAB_GROUP_SYNC_ID), triggeringMotion);
        } else {
            tabActionListener.run(view, propertyModel.get(TabProperties.TAB_ID), triggeringMotion);
        }
    }

    private static void fetchPriceDrop(
            PropertyModel model, Callback<@Nullable PriceDrop> callback, boolean shouldLog) {
        ShoppingPersistedTabDataFetcher fetcher =
                model.get(TabProperties.SHOPPING_PERSISTED_TAB_DATA_FETCHER);
        if (fetcher == null) {
            callback.onResult(null);
            return;
        }
        fetcher.fetch(
                (shoppingPersistedTabData) -> {
                    if (shoppingPersistedTabData == null) {
                        callback.onResult(null);
                        return;
                    }
                    if (shouldLog) {
                        shoppingPersistedTabData.logPriceDropMetrics(SHOPPING_METRICS_IDENTIFIER);
                    }
                    callback.onResult(shoppingPersistedTabData.getPriceDrop());
                });
    }

    private static void onPriceDropFetched(
            ViewLookupCachingFrameLayout rootView,
            PropertyModel model,
            @Nullable PriceDrop priceDrop) {
        if (TabUiUtils.isDataSharingFunctionalityEnabled()) {
            // TODO(crbug.com/361169665): Do activity updates or price drops take priority. Assume
            // activity updates win for now.
            if (model.get(TabProperties.TAB_CARD_LABEL_DATA) != null) return;

            if (priceDrop == null) {
                updateTabCardLabel(rootView, null);
                return;
            }

            TextResolver contentDescriptionResolver =
                    (context) -> {
                        return context.getString(
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
            PriceCardView priceCardView = rootView.fastFindViewById(R.id.price_info_box_outer);
            if (priceDrop == null) {
                priceCardView.setVisibility(GONE);
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

    private static void updateThumbnail(
            ViewLookupCachingFrameLayout view,
            PropertyModel model,
            boolean onlyUpdateIfPlaceholder) {
        TabThumbnailView thumbnail = view.fastFindViewById(R.id.tab_thumbnail);

        // To GC on hide set a background color and remove the thumbnail.
        final boolean isSelected = model.get(TabProperties.IS_SELECTED);
        thumbnail.updateThumbnailPlaceholder(
                model.get(TabProperties.IS_INCOGNITO),
                isSelected,
                model.get(TabProperties.TAB_GROUP_CARD_COLOR));

        final ThumbnailFetcher fetcher = model.get(TabProperties.THUMBNAIL_FETCHER);
        final Size cardSize = model.get(TabProperties.GRID_CARD_SIZE);
        if (fetcher == null || cardSize == null) {
            thumbnail.setImageDrawable(null);
            return;
        }

        if (onlyUpdateIfPlaceholder && !thumbnail.isPlaceholder()) return;

        // TODO(crbug.com/40882123): Consider unsetting the bitmap early to allow memory reuse if
        // needed.
        final Size thumbnailSize = TabUtils.deriveThumbnailSize(cardSize, view.getContext());
        // This callback will be made cancelable inside ThumbnailFetcher so only the latest fetch
        // request will return. When the fetcher is replaced any outbound requests are first
        // canceled inside TabListMediator so it is not necessary to do any sort of validation that
        // the callback matches the current thumbnail fetcher and grid card size.
        Callback<@Nullable Drawable> callback =
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
     * Update the favicon drawable to use from {@link TabFavicon}, and the padding around it. The
     * color work is already handled when favicon is bind in {@link #bindCommonProperties}.
     */
    private static void updateFavicon(ViewLookupCachingFrameLayout rootView, PropertyModel model) {
        final TabFaviconFetcher fetcher = model.get(TabProperties.FAVICON_FETCHER);
        ImageView faviconView = rootView.fastFindViewById(R.id.tab_favicon);
        if (fetcher == null) {
            faviconView.setVisibility(GONE);
            setFavicon(faviconView, model, null);
            return;
        }

        faviconView.setVisibility(View.VISIBLE);
        fetcher.fetch(
                tabFavicon -> {
                    if (fetcher != model.get(TabProperties.FAVICON_FETCHER)) return;

                    setFavicon(faviconView, model, tabFavicon);
                });
    }

    /**
     * Set the favicon drawable to use from {@link TabFavicon}, and the padding around it. The color
     * work is already handled when favicon is bind in {@link #bindCommonProperties}.
     */
    private static void setFavicon(
            ImageView faviconView, PropertyModel model, @Nullable TabFavicon favicon) {
        if (favicon == null) {
            faviconView.setImageDrawable(null);
            return;
        }

        boolean isSelected = model.get(TabProperties.IS_SELECTED);
        faviconView.setImageDrawable(
                isSelected ? favicon.getSelectedDrawable() : favicon.getDefaultDrawable());
    }

    /**
     * Bind color updates.
     *
     * @param rootView The root view of the item.
     * @param isIncognito Whether the model is in incognito mode.
     * @param isSelected Whether the item is selected.
     * @param colorId Color chosen by user for the TabGroup, null if not a tab group.
     */
    private static void updateColor(
            ViewLookupCachingFrameLayout rootView,
            boolean isIncognito,
            boolean isSelected,
            @Nullable @TabGroupColorId Integer colorId) {
        View cardView = rootView.fastFindViewById(R.id.card_view);
        TextView titleView = rootView.fastFindViewById(R.id.tab_title);
        TabThumbnailView thumbnail = rootView.fastFindViewById(R.id.tab_thumbnail);
        ChromeImageView backgroundView = rootView.fastFindViewById(R.id.background_view);
        ImageView mediaIndicator = rootView.fastFindViewById(R.id.media_indicator_icon);

        cardView.getBackground().mutate();
        final @ColorInt int backgroundColor =
                TabCardThemeUtil.getCardViewBackgroundColor(
                        cardView.getContext(), isIncognito, isSelected, colorId);
        ViewCompat.setBackgroundTintList(
                cardView,
                TabCardThemeUtil.getCardViewBackgroundColorStateList(
                        cardView.getContext(), isIncognito, backgroundColor));

        titleView.setTextColor(
                TabCardThemeUtil.getTitleTextColor(
                        titleView.getContext(), isIncognito, isSelected, colorId));

        thumbnail.updateThumbnailPlaceholder(isIncognito, isSelected, colorId);

        ViewCompat.setBackgroundTintList(
                backgroundView,
                TabUiThemeProvider.getHoveredCardBackgroundTintList(
                        backgroundView.getContext(), isIncognito, isSelected));

        mediaIndicator.setImageTintList(
                TabCardThemeUtil.getMediaIndicatorColorStateList(
                        mediaIndicator.getContext(), isIncognito, isSelected));
    }

    private static void updateColorForSelectionToggleButton(
            ViewLookupCachingFrameLayout rootView, boolean isIncognito, boolean isSelected) {
        ImageView actionButton = rootView.fastFindViewById(R.id.action_button);

        Context context = rootView.getContext();
        Resources res = rootView.getResources();
        actionButton
                .getBackground()
                .setLevel(TabCardViewBinderUtils.getCheckmarkLevel(res, isSelected));
        DrawableCompat.setTintList(
                actionButton.getBackground().mutate(),
                TabCardThemeUtil.getToggleActionButtonBackgroundTintList(
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
        @Nullable ViewStub stub = rootView.fastFindViewById(R.id.tab_card_label_stub);
        TabCardLabelView labelView;
        if (stub != null) {
            if (tabCardLabelData == null) return;

            labelView = (TabCardLabelView) stub.inflate();
        } else {
            labelView = rootView.fastFindViewById(R.id.tab_card_label);
        }
        labelView.setData(tabCardLabelData);
    }

    private static @StringRes int getTabContentDescriptionStringId(
            boolean isPinned, @MediaState int mediaState) {
        switch (mediaState) {
            case MediaState.MUTED:
                return isPinned
                        ? R.string.accessibility_tabstrip_tab_pinned_muted
                        : R.string.accessibility_tabstrip_tab_muted;
            case MediaState.AUDIBLE:
                return isPinned
                        ? R.string.accessibility_tabstrip_tab_pinned_audible
                        : R.string.accessibility_tabstrip_tab_audible;
            case MediaState.RECORDING:
                return isPinned
                        ? R.string.accessibility_tabstrip_tab_pinned_recording
                        : R.string.accessibility_tabstrip_tab_recording;
            case MediaState.SHARING:
                return isPinned
                        ? R.string.accessibility_tabstrip_tab_pinned_sharing
                        : R.string.accessibility_tabstrip_tab_sharing;
            case MediaState.NONE:
            default:
                return isPinned
                        ? R.string.accessibility_tabstrip_tab_pinned
                        : R.string.accessibility_tabstrip_tab;
        }
    }

    static void setThumbnailFetcherForTesting(ThumbnailFetcher fetcher) {
        sThumbnailFetcherForTesting = fetcher;
        ResettersForTesting.register(() -> sThumbnailFetcherForTesting = null);
    }
}
