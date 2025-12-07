// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.LIST_CONTAINER_VIEW_DELEGATE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.MAIN_BOTTOM_SHEET_FEED_SECTION_SUBTITLE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.MAIN_BOTTOM_SHEET_MVT_SECTION_SUBTITLE;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.MAIN_BOTTOM_SHEET_NTP_CARDS_SECTION_SUBTITLE_RES_ID;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Class responsible for binding a delegate to a {@link BottomSheetListContainerView}. The delegate
 * provides list content and event handlers to the list container view.
 */
@NullMarked
public class BottomSheetListContainerViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == LIST_CONTAINER_VIEW_DELEGATE) {
            ListContainerViewDelegate delegate = model.get(LIST_CONTAINER_VIEW_DELEGATE);
            if (delegate == null) {
                ((ListContainerView) view).destroy();
            } else {
                ((ListContainerView) view).renderAllListItems(delegate);
            }
        } else if (propertyKey == MAIN_BOTTOM_SHEET_FEED_SECTION_SUBTITLE) {
            BottomSheetListItemView feedListItem = view.findViewById(R.id.feed_settings);
            // The feedListItem could be null if Feeds is disabled. See https://crbug.com/426191805.
            if (feedListItem == null) return;

            String subtitleText =
                    view.getContext().getString(model.get(MAIN_BOTTOM_SHEET_FEED_SECTION_SUBTITLE));
            feedListItem.setSubtitle(subtitleText);
        } else if (propertyKey == MAIN_BOTTOM_SHEET_MVT_SECTION_SUBTITLE) {
            BottomSheetListItemView mvtListItem = view.findViewById(R.id.mvt_settings);
            if (mvtListItem == null) return;

            String subtitleText =
                    view.getContext().getString(model.get(MAIN_BOTTOM_SHEET_MVT_SECTION_SUBTITLE));
            mvtListItem.setSubtitle(subtitleText);
        } else if (propertyKey == MAIN_BOTTOM_SHEET_NTP_CARDS_SECTION_SUBTITLE_RES_ID) {
            BottomSheetListItemView ntpCardsListItem = view.findViewById(R.id.ntp_cards);
            if (ntpCardsListItem == null) return;

            String subtitleText =
                    view.getContext()
                            .getString(
                                    model.get(MAIN_BOTTOM_SHEET_NTP_CARDS_SECTION_SUBTITLE_RES_ID));
            ntpCardsListItem.setSubtitle(subtitleText);
        }
    }
}
