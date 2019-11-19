// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards;

import android.content.res.Resources;
import android.support.v7.widget.RecyclerView;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewGroup;

import androidx.annotation.CallSuper;

import org.chromium.chrome.browser.native_page.ContextMenuManager;
import org.chromium.chrome.browser.native_page.ContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.suggestions.SuggestionsRecyclerView;
import org.chromium.chrome.browser.ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.chrome.browser.ui.widget.displaystyle.UiConfig;

/**
 * Holder for a generic card.
 *
 * Specific behaviors added to the cards:
 *
 * - Tap events will be routed through {@link #onCardTapped()} for subclasses to override.
 *
 * - Cards will get some lateral margins when the viewport is sufficiently wide.
 *   (see {@link HorizontalDisplayStyle#WIDE})
 *
 * Note: If a subclass overrides {@link #onBindViewHolder()}, it should call the
 * parent implementation to reset the private state when a card is recycled.
 */
public abstract class CardViewHolder
        extends NewTabPageViewHolder implements ContextMenuManager.Delegate {
    protected final SuggestionsRecyclerView mRecyclerView;

    protected final UiConfig mUiConfig;

    /**
     * @param layoutId resource id of the layout to inflate and to use as card.
     * @param recyclerView ViewGroup that will contain the newly created view.
     * @param uiConfig The NTP UI configuration object used to adjust the card UI.
     * @param contextMenuManager The manager responsible for the context menu.
     */
    public CardViewHolder(int layoutId, final SuggestionsRecyclerView recyclerView,
            UiConfig uiConfig, final ContextMenuManager contextMenuManager) {
        super(inflateView(layoutId, recyclerView));

        Resources resources = recyclerView.getResources();

        mRecyclerView = recyclerView;

        itemView.setOnClickListener(v -> onCardTapped());

        itemView.setOnCreateContextMenuListener(
                (menu, view, menuInfo)
                        -> contextMenuManager.createContextMenu(
                                menu, itemView, CardViewHolder.this));

        mUiConfig = uiConfig;
    }

    @Override
    public boolean isItemSupported(@ContextMenuItemId int menuItemId) {
        return menuItemId == ContextMenuManager.ContextMenuItemId.REMOVE && isDismissable();
    }

    @Override
    public void removeItem() {
        getRecyclerView().dismissItemWithAnimation(this);
    }

    @Override
    public void openItem(int windowDisposition) {
        throw new UnsupportedOperationException();
    }

    @Override
    public String getUrl() {
        return null;
    }

    @Override
    public String getContextMenuTitle() {
        return null;
    }

    @Override
    public boolean isDismissable() {
        int position = getAdapterPosition();
        if (position == RecyclerView.NO_POSITION) return false;

        return !mRecyclerView.getNewTabPageAdapter().getItemDismissalGroup(position).isEmpty();
    }

    @Override
    public void onContextMenuCreated() {}

    /**
     * Called when the NTP cards adapter is requested to update the currently visible ViewHolder
     * with data.
     */
    @CallSuper
    public void onBindViewHolder() {
        // Reset the transparency and translation in case a dismissed card is being recycled.
        itemView.setAlpha(1f);
        itemView.setTranslationX(0f);

        itemView.addOnAttachStateChangeListener(new OnAttachStateChangeListener() {
            @Override
            public void onViewAttachedToWindow(View view) {}

            @Override
            public void onViewDetachedFromWindow(View view) {
                // In some cases a view can be removed while a user is interacting with it, without
                // calling ItemTouchHelper.Callback#clearView(), which we rely on for bottomSpacer
                // calculations. So we call this explicitly here instead.
                // See https://crbug.com/664466, b/32900699
                mRecyclerView.onItemDismissFinished(mRecyclerView.findContainingViewHolder(view));
                itemView.removeOnAttachStateChangeListener(this);
            }
        });

        // Make sure we use the right background.
        updateLayoutParams();
    }

    /**
     * Override this to react when the card is tapped. This method will not be called if the card is
     * currently peeking.
     */
    protected void onCardTapped() {}

    private static View inflateView(int resourceId, ViewGroup parent) {
        return LayoutInflater.from(parent.getContext()).inflate(resourceId, parent, false);
    }

    public static boolean isCard(@ItemViewType int type) {
        switch (type) {
            case ItemViewType.SNIPPET:
            case ItemViewType.STATUS:
            case ItemViewType.ACTION:
            case ItemViewType.PROMO:
                return true;
            case ItemViewType.ABOVE_THE_FOLD:
            case ItemViewType.HEADER:
            case ItemViewType.PROGRESS:
            case ItemViewType.FOOTER:
                return false;
        }
        assert false;
        return false;
    }

    public SuggestionsRecyclerView getRecyclerView() {
        return mRecyclerView;
    }
}
