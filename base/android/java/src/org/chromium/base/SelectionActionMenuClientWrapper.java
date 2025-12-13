// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import static java.lang.annotation.ElementType.TYPE_USE;

import android.content.Context;
import android.content.pm.ResolveInfo;
import android.view.MenuItem;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.List;

/**
 * Provides the capability to customize the text selection menu (see SelectionActionMenuDelegate).
 */
// TODO(crbug.com/413195996): Remove this interface once it's no longer needed.
@NullMarked
public interface SelectionActionMenuClientWrapper {
    @IntDef({MenuType.FLOATING, MenuType.DROPDOWN})
    @Retention(RetentionPolicy.SOURCE)
    @interface MenuType {
        int FLOATING = 0;
        int DROPDOWN = 1;
    }

    @IntDef({
        DefaultItem.CUT,
        DefaultItem.COPY,
        DefaultItem.PASTE,
        DefaultItem.PASTE_AS_PLAIN_TEXT,
        DefaultItem.SHARE,
        DefaultItem.SELECT_ALL,
        DefaultItem.WEB_SEARCH
    })
    @Target(TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    @interface DefaultItem {
        int CUT = 1;
        int COPY = 2;
        int PASTE = 3;
        int PASTE_AS_PLAIN_TEXT = 4;
        int SHARE = 5;
        int SELECT_ALL = 6;
        int WEB_SEARCH = 7;
    }

    @DefaultItem
    int[] getDefaultMenuItemOrder(@MenuType int menuType);

    List<MenuItem> getAdditionalMenuItems(
            Context context,
            @MenuType int menuType,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            String selectedText);

    List<ResolveInfo> filterTextProcessingActivities(
            Context context, @MenuType int menuType, List<ResolveInfo> activities);

    boolean handleMenuItemClick(Context context, MenuItem item);
}
