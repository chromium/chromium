// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.CompoundButton;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Hosts the properties for the improved bookmarks save flow. */
public class ImprovedBookmarkSaveFlowProperties {
    /** Encapsulates the display text for folders. */
    static class FolderText {
        private final String mFolderDisplayText;
        private final int mFolderTitleStartIndex;
        private final int mFolderTitleEndIndex;

        FolderText(String folderDisplayText, int folderTitleStartIndex, int folderTitleLength) {
            mFolderDisplayText = folderDisplayText;
            mFolderTitleStartIndex = folderTitleStartIndex;
            mFolderTitleEndIndex = mFolderTitleStartIndex + folderTitleLength;
        }

        String getDisplayText() {
            return mFolderDisplayText;
        }

        int getFolderTitleStartIndex() {
            return mFolderTitleStartIndex;
        }

        int getFolderTitleEndIndex() {
            return mFolderTitleEndIndex;
        }
    }

    public static final WritableObjectPropertyKey<View.OnClickListener>
            BOOKMARK_ROW_CLICK_LISTENER = new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Drawable> BOOKMARK_ROW_ICON =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<FolderText> FOLDER_TEXT =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey PRICE_TRACKING_VISIBLE =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey PRICE_TRACKING_ENABLED =
            new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey PRICE_TRACKING_SWITCH_CHECKED =
            new WritableBooleanPropertyKey();
    public static final WritableObjectPropertyKey<CompoundButton.OnCheckedChangeListener>
            PRICE_TRACKING_SWITCH_LISTENER = new WritableObjectPropertyKey<>();

    private static final PropertyKey[] NEW_KEYS = {
        BOOKMARK_ROW_CLICK_LISTENER,
        BOOKMARK_ROW_ICON,
        FOLDER_TEXT,
        PRICE_TRACKING_VISIBLE,
        PRICE_TRACKING_ENABLED,
        PRICE_TRACKING_SWITCH_CHECKED,
        PRICE_TRACKING_SWITCH_LISTENER
    };
    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(BookmarkSaveFlowProperties.ALL_KEYS, NEW_KEYS);
}
