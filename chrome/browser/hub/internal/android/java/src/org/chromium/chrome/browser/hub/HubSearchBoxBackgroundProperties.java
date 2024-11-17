// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/** Model for controlling hub search's background properties. */
public class HubSearchBoxBackgroundProperties {
    /** The color scheme that the search box background should have. */
    public static final WritableIntPropertyKey COLOR_SCHEME = new WritableIntPropertyKey();

    /** An indicator of whether the background should be visible. */
    public static final WritableBooleanPropertyKey SHOW_BACKGROUND =
            new WritableBooleanPropertyKey();

    /** Creates a model for a the cross device pane. */
    public static PropertyModel create() {
        return new PropertyModel.Builder(ALL_KEYS).build();
    }

    public static final PropertyKey[] ALL_KEYS = {COLOR_SCHEME, SHOW_BACKGROUND};
}
