// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import android.content.Context;
import android.view.View;
import android.widget.CompoundButton;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

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

    /**
     * Returns the resource id for the content description of the list item's trailing icon for the
     * given type.
     *
     * @param type The type of the list item.
     */
    @Nullable
    @StringRes
    Integer getTrailingIconDescriptionResId(int type);

    /**
     * Returns the initial checked state for a list item that contains a switch. The default
     * implementation returns false.
     *
     * @param type The type of the list item.
     * @return True if the item's switch should be checked, false otherwise.
     */
    default boolean isListItemChecked(int type) {
        return false;
    }

    /**
     * Returns the listener to be invoked when a list item's checked state changes. The default
     * implementation returns null.
     *
     * @param type The type of the list item for which to get the listener.
     * @return The OnCheckedChangeListener for the item, or null if none is needed.
     */
    default CompoundButton.@Nullable OnCheckedChangeListener getOnCheckedChangeListener(int type) {
        return null;
    }
}
