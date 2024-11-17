// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** List of properties used by TabGridSecondaryItem. */
class ResizableMessageCardViewProperties {
    public static final PropertyModel.WritableIntPropertyKey WIDTH =
            new PropertyModel.WritableIntPropertyKey();

    private static final PropertyKey[] KEYS = new PropertyKey[] {WIDTH};
    public static final PropertyKey[] ALL_KEYS =
            PropertyModel.concatKeys(KEYS, MessageCardViewProperties.ALL_KEYS);
}
