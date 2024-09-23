// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Model for cross device pane hosting a list of group items. */
public class CrossDeviceListProperties {
    /** An indicator of whether the empty state should be visible. */
    public static final WritableBooleanPropertyKey EMPTY_STATE_VISIBLE =
            new WritableBooleanPropertyKey();

    /** Creates a model for a the cross device pane. */
    public static PropertyModel create() {
        return new PropertyModel.Builder(ALL_KEYS).build();
    }

    public static final PropertyKey[] ALL_KEYS = {EMPTY_STATE_VISIBLE};
}
