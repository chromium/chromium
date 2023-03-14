// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.annotation.SuppressLint;
import android.view.MotionEvent;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Responsible for binding views to their properties. */
class BookmarkManagerViewBinder {
    static void bindPersonalizedPromoView(PropertyModel model, View view, PropertyKey key) {
        if (key == BookmarkManagerProperties.BOOKMARK_PROMO_HEADER) {
            PersonalizedSigninPromoView promoView =
                    view.findViewById(org.chromium.chrome.R.id.signin_promo_view_container);
            model.get(BookmarkManagerProperties.BOOKMARK_PROMO_HEADER)
                    .setUpSyncPromoView(promoView);
        }
    }

    static void bindLegacyPromoView(PropertyModel model, View view, PropertyKey key) {}

    static void bindSectionHeaderView(PropertyModel model, View view, PropertyKey key) {
        if (key == BookmarkManagerProperties.BOOKMARK_LIST_ENTRY) {
            BookmarkListEntry bookmarkListEntry =
                    model.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY);
            TextView title = view.findViewById(org.chromium.chrome.R.id.title);
            title.setText(bookmarkListEntry.getHeaderTitle());
            if (bookmarkListEntry.getSectionHeaderData().topPadding > 0) {
                title.setPaddingRelative(title.getPaddingStart(),
                        bookmarkListEntry.getSectionHeaderData().topPadding, title.getPaddingEnd(),
                        title.getPaddingBottom());
            }
        }
    }

    static void bindBookmarkFolderView(PropertyModel model, View view, PropertyKey key) {
        bindBookmarkEntryView(model, view, key);
    }

    static void bindBookmarkItemView(PropertyModel model, View view, PropertyKey key) {
        bindBookmarkEntryView(model, view, key);
    }

    static void bindShoppingItemView(PropertyModel model, View view, PropertyKey key) {
        bindBookmarkEntryView(model, view, key);
    }

    @SuppressLint("ClickableViewAccessibility")
    private static void bindBookmarkEntryView(PropertyModel model, View view, PropertyKey key) {
        if (key == BookmarkManagerProperties.BOOKMARK_ID) {
            // Also uses BookmarkManagerProperties.LOCATION and
            // BookmarkManagerProperties.IS_FROM_FILTER_VIEW.
            BookmarkRow row = ((BookmarkRow) view);
            BookmarkId id = model.get(BookmarkManagerProperties.BOOKMARK_ID);
            row.setBookmarkId(id, model.get(BookmarkManagerProperties.LOCATION),
                    model.get(BookmarkManagerProperties.IS_FROM_FILTER_VIEW));
        } else if (key == BookmarkManagerProperties.ITEM_TOUCH_HELPER) {
            // Also uses BookmarkManagerProperties.VIEW_HOLDER.
            BookmarkRow row = ((BookmarkRow) view);
            row.setDragHandleOnTouchListener((v, event) -> {
                if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                    model.get(BookmarkManagerProperties.ITEM_TOUCH_HELPER)
                            .startDrag(model.get(BookmarkManagerProperties.VIEW_HOLDER));
                }
                // This callback consumed the click action (don't activate menu).
                return true;
            });
        } else if (key == BookmarkManagerProperties.IS_HIGHLIGHTED) {
            // Also uses key == BookmarkManagerProperties.CLEAR_HIGHLIGHT.
            // Turn on the highlight for the currently highlighted bookmark.
            if (model.get(BookmarkManagerProperties.IS_HIGHLIGHTED)) {
                HighlightParams params = new HighlightParams(HighlightShape.RECTANGLE);
                params.setNumPulses(1);
                ViewHighlighter.turnOnHighlight(view, params);
                model.get(BookmarkManagerProperties.CLEAR_HIGHLIGHT).run();
            } else {
                // We need this in case we are change state during a pulse.
                ViewHighlighter.turnOffHighlight(view);
            }
        }
    }

    static void bindDividerView(PropertyModel model, View view, PropertyKey key) {}

    static void bindShoppingFilterView(PropertyModel model, View view, PropertyKey key) {
        if (key == BookmarkManagerProperties.OPEN_FOLDER) {
            LinearLayout layout = (LinearLayout) view;
            layout.setClickable(true);
            layout.setOnClickListener((v) -> {
                model.get(BookmarkManagerProperties.OPEN_FOLDER)
                        .onResult(BookmarkId.SHOPPING_FOLDER);
            });
        }
    }
}
