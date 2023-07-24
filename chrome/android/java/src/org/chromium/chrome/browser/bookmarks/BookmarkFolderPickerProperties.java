// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Properties for the folder picker activity. */
class BookmarkFolderPickerProperties {
    @IntDef({ItemType.NORMAL})
    @Retention(RetentionPolicy.SOURCE)
    @interface ItemType {
        int NORMAL = 0;
    }

    static final WritableObjectPropertyKey<String> TOOLBAR_TITLE =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<View.OnClickListener> CANCEL_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<View.OnClickListener> MOVE_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    static final WritableBooleanPropertyKey MOVE_BUTTON_ENABLED = new WritableBooleanPropertyKey();
    static final PropertyKey[] ALL_KEYS = {
            TOOLBAR_TITLE, CANCEL_CLICK_LISTENER, MOVE_CLICK_LISTENER, MOVE_BUTTON_ENABLED};
}
