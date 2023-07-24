// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the folder picker activity rows. */
class BookmarkFolderPickerRowProperties {
    static final WritableObjectPropertyKey<ImprovedBookmarkFolderSelectRowCoordinator>
            ROW_COORDINATOR = new WritableObjectPropertyKey<>();
    static final PropertyKey[] ALL_KEYS = {ROW_COORDINATOR};
}
