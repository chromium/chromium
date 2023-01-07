// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sort_ui;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Represents data for the Feed Options pane. */
public class FeedOptionsProperties {
    public static final PropertyModel.WritableBooleanPropertyKey VISIBILITY_KEY =
            new PropertyModel.WritableBooleanPropertyKey();

    static PropertyKey[] getAllKeys() {
        return new PropertyKey[] {VISIBILITY_KEY};
    }
}
