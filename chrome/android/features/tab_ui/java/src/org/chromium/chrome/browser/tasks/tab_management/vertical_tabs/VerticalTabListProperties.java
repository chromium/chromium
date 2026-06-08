// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Properties for the Vertical Tab List. */
@NullMarked
public class VerticalTabListProperties {
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            ON_GRID_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            ON_SEARCH_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyModel.WritableObjectPropertyKey<View.OnClickListener>
            ON_NEW_TAB_CLICK_LISTENER = new PropertyModel.WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                ON_GRID_CLICK_LISTENER, ON_SEARCH_CLICK_LISTENER, ON_NEW_TAB_CLICK_LISTENER
            };
}
