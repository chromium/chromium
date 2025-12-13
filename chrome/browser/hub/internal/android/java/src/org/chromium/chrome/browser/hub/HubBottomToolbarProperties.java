// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubColorMixer.COLOR_MIXER;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** Responsible for holding properties of the bottom toolbar in the hub. */
@NullMarked
class HubBottomToolbarProperties {
    // The visibility of the bottom toolbar.
    public static final WritableBooleanPropertyKey BOTTOM_TOOLBAR_VISIBLE =
            new WritableBooleanPropertyKey();

    // All property keys for the bottom toolbar.
    static final PropertyKey[] ALL_BOTTOM_KEYS = {
        COLOR_MIXER, BOTTOM_TOOLBAR_VISIBLE,
    };
}
