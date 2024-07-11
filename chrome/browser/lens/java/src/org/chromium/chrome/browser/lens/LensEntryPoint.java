// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

import androidx.annotation.IntDef;

@IntDef({
    LensEntryPoint.CONTEXT_MENU_SEARCH_MENU_ITEM,
    LensEntryPoint.CONTEXT_MENU_SHOP_MENU_ITEM,
    LensEntryPoint.CONTEXT_MENU_CHIP,
    LensEntryPoint.OMNIBOX,
    LensEntryPoint.NEW_TAB_PAGE,
    LensEntryPoint.TASKS_SURFACE,
    LensEntryPoint.QUICK_ACTION_SEARCH_WIDGET,
    LensEntryPoint.GOOGLE_BOTTOM_BAR
})
public @interface LensEntryPoint {
    int CONTEXT_MENU_SEARCH_MENU_ITEM = 0;
    int CONTEXT_MENU_SHOP_MENU_ITEM = 1;
    int CONTEXT_MENU_CHIP = 2;
    int OMNIBOX = 3;
    int NEW_TAB_PAGE = 4;
    int TASKS_SURFACE = 5;
    int QUICK_ACTION_SEARCH_WIDGET = 6;
    int GOOGLE_BOTTOM_BAR = 7;
}
