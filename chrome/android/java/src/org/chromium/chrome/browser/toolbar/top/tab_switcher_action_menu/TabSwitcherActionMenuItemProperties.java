// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top.tab_switcher_action_menu;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Data properties for the Tab Switcher Action Menu Item
 */
public class TabSwitcherActionMenuItemProperties {
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey ICON_ID = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey MENU_ID = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {TITLE, ICON_ID, MENU_ID};
}
