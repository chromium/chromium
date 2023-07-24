// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Responsible for hosting properties of the improved bookmark folder select view. */
class ImprovedBookmarkFolderSelectRowProperties {
    static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    static final WritableBooleanPropertyKey END_ICON_VISIBLE = new WritableBooleanPropertyKey();
    static final WritableObjectPropertyKey<View.OnClickListener> ROW_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    static final WritableObjectPropertyKey<ImprovedBookmarkFolderSelectRowCoordinator> COORDINATOR =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
            TITLE, END_ICON_VISIBLE, ROW_CLICK_LISTENER, COORDINATOR};
}
