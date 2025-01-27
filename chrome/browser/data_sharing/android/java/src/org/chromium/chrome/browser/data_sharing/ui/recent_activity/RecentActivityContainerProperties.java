// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.recent_activity;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Properties for the recent activity list container view. */
class RecentActivityContainerProperties {
    public static final WritableBooleanPropertyKey EMPTY_STATE_VISIBLE =
            new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {EMPTY_STATE_VISIBLE};
}
