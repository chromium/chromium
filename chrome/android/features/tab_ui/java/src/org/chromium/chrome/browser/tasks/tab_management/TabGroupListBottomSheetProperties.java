// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Properties for the Tab Group List Bottom Sheet. */
@NullMarked
public class TabGroupListBottomSheetProperties {
    public static final WritableBooleanPropertyKey ADD_TO_GROUP_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        ADD_TO_GROUP_VISIBLE,
    };
}
