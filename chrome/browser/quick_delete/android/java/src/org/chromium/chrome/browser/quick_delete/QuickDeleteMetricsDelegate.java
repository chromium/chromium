// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

/**
 * A delegate class to record metrics associated with {@link QuickDeleteController}.
 */
public class QuickDeleteMetricsDelegate {
    /**
     * These values are persisted to logs. Entries should not be renumbered and
     * numeric values should never be reused.
     *
     * Must be kept in sync with the PrivacyQuickDelete in enums.xml.
     */
    @IntDef({PrivacyQuickDelete.MENU_ITEM_CLICKED, PrivacyQuickDelete.DELETE_CLICKED,
            PrivacyQuickDelete.CANCEL_CLICKED, PrivacyQuickDelete.DIALOG_DISMISSED_IMPLICITLY})
    public @interface PrivacyQuickDelete {
        int MENU_ITEM_CLICKED = 0;
        int DELETE_CLICKED = 1;
        int CANCEL_CLICKED = 2;
        int DIALOG_DISMISSED_IMPLICITLY = 3;
        // Always update MAX_VALUE to match the last item.
        int MAX_VALUE = DIALOG_DISMISSED_IMPLICITLY;
    }

    public static void recordHistogram(@PrivacyQuickDelete int privacyQuickDelete) {
        RecordHistogram.recordEnumeratedHistogram(
                "Privacy.QuickDelete", privacyQuickDelete, PrivacyQuickDelete.MAX_VALUE);
    }
}
