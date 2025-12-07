// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ANIMATION_STATUS;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.tasks.tab_management.TabActionListener;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * A {@link org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder} for the pinned tabs
 * bar item.
 */
@NullMarked
public class PinnedTabStripItemViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        assert view instanceof PinnedTabStripItemView;
        final PinnedTabStripItemView itemView = (PinnedTabStripItemView) view;

        if (TabProperties.FAVICON_FETCHER.equals(propertyKey)) {
            itemView.setFaviconIcon(
                    model.get(TabProperties.FAVICON_FETCHER), model.get(TabProperties.IS_SELECTED));
        } else if (TabProperties.TITLE.equals(propertyKey)) {
            itemView.setTitle(model.get(TabProperties.TITLE));
        } else if (TabProperties.GRID_CARD_SIZE.equals(propertyKey)) {
            itemView.setGridCardSize(model.get(TabProperties.GRID_CARD_SIZE));
        } else if (TabProperties.IS_SELECTED.equals(propertyKey)) {
            boolean isSelected = model.get(TabProperties.IS_SELECTED);
            itemView.setSelected(isSelected, model.get(TabProperties.IS_INCOGNITO));
            itemView.setFaviconIcon(model.get(TabProperties.FAVICON_FETCHER), isSelected);
        } else if (TabProperties.TAB_CLICK_LISTENER.equals(propertyKey)) {
            TabActionListener listener = model.get(TabProperties.TAB_CLICK_LISTENER);
            if (listener == null) return;
            view.setOnClickListener(
                    v -> {
                        listener.run(
                                v, model.get(TabProperties.TAB_ID), /* triggeringMotion= */ null);
                    });
        } else if (TabProperties.TAB_CONTEXT_CLICK_LISTENER == propertyKey) {
            itemView.setNullableContextClickListener(
                    model.get(TabProperties.TAB_CONTEXT_CLICK_LISTENER),
                    view,
                    model.get(TabProperties.TAB_ID));
        } else if (CARD_ANIMATION_STATUS.equals(propertyKey)) {
            itemView.setCardAnimationStatus(model.get(CARD_ANIMATION_STATUS));
        }
    }
}
