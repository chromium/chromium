// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.tray;

import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.Objects;

/**
 * Properties of a single download row in the download toolbar button tray. Shared by the completion
 * popup and the full tray list so both render rows identically.
 */
@NullMarked
class DownloadTrayItemProperties {
    /** A row action button's icon, accessibility labels and callback. */
    static final class ActionButton {
        final @DrawableRes int mIconRes;
        final @StringRes int mDescriptionRes;
        final @StringRes int mTitledDescriptionRes;
        final Callback<ContentId> mOnClick;

        ActionButton(
                @DrawableRes int iconRes,
                @StringRes int descriptionRes,
                @StringRes int titledDescriptionRes,
                Callback<ContentId> onClick) {
            mIconRes = iconRes;
            mDescriptionRes = descriptionRes;
            mTitledDescriptionRes = titledDescriptionRes;
            mOnClick = onClick;
        }

        @Override
        public boolean equals(@Nullable Object obj) {
            if (this == obj) return true;
            if (!(obj instanceof ActionButton other)) return false;
            return mIconRes == other.mIconRes
                    && mDescriptionRes == other.mDescriptionRes
                    && mTitledDescriptionRes == other.mTitledDescriptionRes
                    && Objects.equals(mOnClick, other.mOnClick);
        }

        @Override
        public int hashCode() {
            return Objects.hash(mIconRes, mDescriptionRes, mTitledDescriptionRes, mOnClick);
        }
    }

    /** The view type of the rows in the download tray list, which all share one layout. */
    static final int ROW_TYPE = 0;

    /** The {@link #PROGRESS} of a download whose size, and so how far along it is, is unknown. */
    static final int PROGRESS_INDETERMINATE = -1;

    /** The download's identifier, passed to row action callbacks. */
    static final ReadableObjectPropertyKey<ContentId> ITEM_ID = new ReadableObjectPropertyKey<>();

    /** The file-type icon drawable resource. */
    static final WritableIntPropertyKey ICON = new WritableIntPropertyKey();

    /** The file name. */
    static final WritableObjectPropertyKey<CharSequence> TITLE = new WritableObjectPropertyKey<>();

    /** The status line below the file name, e.g. the download's size and state. */
    static final WritableObjectPropertyKey<CharSequence> DESCRIPTION =
            new WritableObjectPropertyKey<>();

    /** Invoked when the row is clicked, to open the download; null if there is nothing to open. */
    static final WritableObjectPropertyKey<View.@Nullable OnClickListener> ON_OPEN =
            new WritableObjectPropertyKey<>();

    /** Whether the download is in progress, which shows the progress bar. */
    static final WritableBooleanPropertyKey IS_IN_PROGRESS = new WritableBooleanPropertyKey();

    /**
     * How much of a download in progress has been received, from 0 to 100, or {@link
     * #PROGRESS_INDETERMINATE}, as desktop's progress bar value.
     */
    static final WritableIntPropertyKey PROGRESS = new WritableIntPropertyKey();

    /** Whether the download is paused. */
    static final WritableBooleanPropertyKey IS_PAUSED = new WritableBooleanPropertyKey();

    /** The primary action button (pause, resume or retry), or null if the row has none. */
    static final WritableObjectPropertyKey<@Nullable ActionButton> PRIMARY_ACTION =
            new WritableObjectPropertyKey<>();

    /** The secondary action button (cancel), or null if the row has none. */
    static final WritableObjectPropertyKey<@Nullable ActionButton> SECONDARY_ACTION =
            new WritableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = {
        ITEM_ID,
        ICON,
        TITLE,
        DESCRIPTION,
        ON_OPEN,
        IS_IN_PROGRESS,
        PROGRESS,
        IS_PAUSED,
        PRIMARY_ACTION,
        SECONDARY_ACTION,
    };

    private DownloadTrayItemProperties() {}
}
