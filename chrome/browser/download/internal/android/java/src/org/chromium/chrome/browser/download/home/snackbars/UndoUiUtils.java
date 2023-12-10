// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.snackbars;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Collection;
import java.util.Locale;

/** Helpers for building components or strings of the download deletion undo UI. */
final class UndoUiUtils {
    private UndoUiUtils() {}

    /** @return A {@link String} representing the title text for an undo snackbar. */
    public static String getTitleFor(Collection<OfflineItem> items) {
        return items.size() == 1
                ? items.iterator().next().title
                : String.format(Locale.getDefault(), "%d", items.size());
    }

    /** @return A {@link String} representing the template text for an undo snackbar. */
    public static String getTemplateTextFor(Collection<OfflineItem> items) {
        Context context = ContextUtils.getApplicationContext();
        return items.size() == 1
                ? context.getString(R.string.delete_message)
                : context.getString(R.string.undo_bar_multiple_downloads_delete_message);
    }

    /** @return A {@link String} representing the text to announce when an undo occurs. */
    public static String getAccessibilityActionAnnouncementTextFor(Collection<OfflineItem> items) {
        String title = getTitleFor(items);
        Context context = ContextUtils.getApplicationContext();
        return items.size() == 1
                ? context.getString(R.string.undo_bar_delete_restore_accessibility_message, title)
                : context.getString(
                        R.string.undo_bar_multiple_downloads_delete_restore_accessibility_message,
                        title);
    }
}
