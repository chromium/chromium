// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.FEED;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.MAIN;
import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;

import org.chromium.build.annotations.NullMarked;

/** Utility class of the NTP customization. */
@NullMarked
public class NtpCustomizationUtils {

    /**
     * Every list in NTP customization bottom sheets should use this function to get the background
     * for its list items.
     *
     * @param size The number of the list items to be displayed in a container view.
     * @param index The index of the currently iterated list item.
     * @return The background of the list item view at the given index.
     */
    public static int getBackground(int size, int index) {
        if (size == 1) {
            return R.drawable.ntp_customization_bottom_sheet_list_item_background_single;
        }

        if (index == 0) {
            return R.drawable.ntp_customization_bottom_sheet_list_item_background_top;
        }

        if (index == size - 1) {
            return R.drawable.ntp_customization_bottom_sheet_list_item_background_bottom;
        }

        return R.drawable.ntp_customization_bottom_sheet_list_item_background_middle;
    }

    /**
     * Returns the resource ID of the content description for the bottom sheet. The main bottom
     * sheet's content description requires special handling beyond this function.
     */
    public static int getSheetContentDescription(
            @NtpCustomizationCoordinator.BottomSheetType int type) {
        switch (type) {
            case NTP_CARDS:
                return R.string.ntp_customization_ntp_cards_bottom_sheet;
            case FEED:
                return R.string.ntp_customization_feed_bottom_sheet;
            default:
                assert false : "Bottom sheet type not supported!";
                return -1;
        }
    }

    /**
     * Returns the resource ID for the accessibility string announced when the bottom sheet is fully
     * expanded.
     */
    public static int getSheetFullHeightAccessibilityStringId(
            @NtpCustomizationCoordinator.BottomSheetType int type) {
        switch (type) {
            case MAIN:
                return R.string.ntp_customization_main_bottom_sheet_opened_full;
            case NTP_CARDS:
                return R.string.ntp_customization_ntp_cards_bottom_sheet_opened_full;
            case FEED:
                return R.string.ntp_customization_feed_bottom_sheet_opened_full;
            default:
                assert false : "Bottom sheet type not supported!";
                return -1;
        }
    }
}
