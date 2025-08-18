// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.pinned_tabs_strip;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/** Properties for the pinned tabs strip. */
@NullMarked
class PinnedTabStripProperties {
    public static final WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();
    public static final WritableIntPropertyKey SCROLL_TO_POSITION = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {IS_VISIBLE, SCROLL_TO_POSITION};
}
