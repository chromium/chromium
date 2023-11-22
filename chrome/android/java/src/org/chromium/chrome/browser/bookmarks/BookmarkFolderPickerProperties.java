// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the folder picker activity. */
class BookmarkFolderPickerProperties {
    static final WritableObjectPropertyKey<String> TOOLBAR_TITLE =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Runnable> CANCEL_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<Runnable> MOVE_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    static final WritableBooleanPropertyKey MOVE_BUTTON_ENABLED = new WritableBooleanPropertyKey();
    // Using WritableObjectPropertyKey and skipEquality=true here because the menu button is
    // initialized by the activity. Since we have no control over it, it could get instantiated
    // after the property is already set.
    static final WritableObjectPropertyKey<Boolean> ADD_NEW_FOLDER_BUTTON_ENABLED =
            new WritableObjectPropertyKey<>(/* skipEquality= */ true);

    static final PropertyKey[] ALL_KEYS = {
        TOOLBAR_TITLE,
        CANCEL_CLICK_LISTENER,
        MOVE_CLICK_LISTENER,
        MOVE_BUTTON_ENABLED,
        ADD_NEW_FOLDER_BUTTON_ENABLED
    };
}
