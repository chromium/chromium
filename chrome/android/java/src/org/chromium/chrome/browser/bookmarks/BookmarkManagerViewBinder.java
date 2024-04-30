// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.res.Resources;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.TextView;

import androidx.annotation.DimenRes;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.SectionHeaderData;
import org.chromium.chrome.browser.ui.signin.PersonalizedSigninPromoView;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Responsible for binding views to their properties. */
class BookmarkManagerViewBinder {
    static void bindPersonalizedPromoView(PropertyModel model, View view, PropertyKey key) {
        if (key == BookmarkManagerProperties.BOOKMARK_PROMO_HEADER) {
            PersonalizedSigninPromoView promoView =
                    view.findViewById(R.id.signin_promo_view_container);
            model.get(BookmarkManagerProperties.BOOKMARK_PROMO_HEADER)
                    .setUpSyncPromoView(promoView);
        } else if (key == BookmarkManagerProperties.PROMO_TOP_MARGIN_RES) {
            final @DimenRes int topMarginRes =
                    model.get(BookmarkManagerProperties.PROMO_TOP_MARGIN_RES);
            if (topMarginRes != Resources.ID_NULL) {
                Resources resources = view.getResources();
                int topMarginPx = resources.getDimensionPixelSize(topMarginRes);

                PersonalizedSigninPromoView promoView =
                        view.findViewById(R.id.signin_promo_view_container);
                MarginLayoutParams layoutParams = (MarginLayoutParams) promoView.getLayoutParams();
                layoutParams.setMargins(
                        layoutParams.leftMargin,
                        topMarginPx,
                        layoutParams.rightMargin,
                        layoutParams.bottomMargin);
                promoView.setLayoutParams(layoutParams);
            }
        }
    }

    static void bindLegacyPromoView(PropertyModel model, View view, PropertyKey key) {}

    static void bindSectionHeaderView(PropertyModel model, View view, PropertyKey key) {
        if (key == BookmarkManagerProperties.BOOKMARK_LIST_ENTRY) {
            Resources resources = view.getResources();
            BookmarkListEntry bookmarkListEntry =
                    model.get(BookmarkManagerProperties.BOOKMARK_LIST_ENTRY);
            TextView title = view.findViewById(R.id.title);
            SectionHeaderData sectionHeaderData = bookmarkListEntry.getSectionHeaderData();
            title.setText(resources.getText(sectionHeaderData.titleRes));
            final @DimenRes int topPaddingRes = sectionHeaderData.topPaddingRes;
            if (topPaddingRes != Resources.ID_NULL) {
                title.setPaddingRelative(
                        title.getPaddingStart(),
                        resources.getDimensionPixelSize(topPaddingRes),
                        title.getPaddingEnd(),
                        title.getPaddingBottom());
            }
        }
    }

    static void bindDividerView(PropertyModel model, View view, PropertyKey key) {}
}
