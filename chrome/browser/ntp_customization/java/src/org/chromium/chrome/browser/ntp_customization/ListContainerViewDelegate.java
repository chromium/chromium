// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import android.content.Context;
import android.support.annotation.DrawableRes;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.List;

/**
 * This delegate provides list content and event handlers to {@link BottomSheetListContainerView}.
 */
@NullMarked
public interface ListContainerViewDelegate {
    /**
     * Returns the types of items to be added to the container view. The size of the returned list
     * should be the same as the number of the list items to be displayed.
     */
    List<Integer> getListItems();

    /**
     * Returns the view id to be assigned to the list item for the given type.
     *
     * @param type The type of the list item.
     */
    default int getListItemId(int type) {
        return View.NO_ID;
    }

    /**
     * Returns the title to be displayed in the the list item for the given type.
     *
     * @param type The type of the list item.
     */
    String getListItemTitle(int type, Context context);

    /**
     * Returns the subtitle to be displayed in the list item for the given type.
     *
     * @param type The type of the list item.
     */
    @Nullable
    String getListItemSubtitle(int type, Context context);

    /**
     * Returns the listener associated with the given type of the list item.
     *
     * @param type The type of the list item.
     */
    View.@Nullable OnClickListener getListener(int type);

    /**
     * Returns the resource id of the trailing icon in the list item for the given type.
     *
     * @param type The type of the list item.
     */
    @Nullable
    @DrawableRes
    Integer getTrailingIcon(int type);
}
