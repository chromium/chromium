// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

/** Utility class of the NTP customization. */
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
}
