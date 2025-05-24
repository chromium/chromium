// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.snackbars;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.components.offline_items_collection.OfflineItem;

import java.util.Collection;
import java.util.Locale;

/** Helpers for building components or strings of the download deletion undo UI. */
@NullMarked
final class UndoUiUtils {
    private UndoUiUtils() {}

    /** @return A {@link String} representing the title text for an undo snackbar. */
    public static @Nullable String getTitleFor(Collection<OfflineItem> items) {
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
}
