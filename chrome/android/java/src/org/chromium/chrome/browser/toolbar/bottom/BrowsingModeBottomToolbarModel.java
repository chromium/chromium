// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import org.chromium.ui.modelutil.PropertyModel;

/**
 * All of the state for the bottom toolbar, updated by the {@link
 * BrowsingModeBottomToolbarCoordinator}.
 */
public class BrowsingModeBottomToolbarModel extends PropertyModel {
    /** Primary color of bottom toolbar. */
    static final WritableIntPropertyKey PRIMARY_COLOR = new WritableIntPropertyKey();

    /** Whether the browsing mode bottom toolbar is visible */
    static final WritableBooleanPropertyKey IS_VISIBLE = new WritableBooleanPropertyKey();

    /** Default constructor. */
    BrowsingModeBottomToolbarModel() {
        super(IS_VISIBLE, PRIMARY_COLOR);
        set(IS_VISIBLE, true);
    }
}
