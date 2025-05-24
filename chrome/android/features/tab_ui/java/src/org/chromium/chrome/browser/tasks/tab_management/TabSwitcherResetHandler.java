// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;

import java.util.List;

/** Interface to delegate resetting the tab grid. */
@NullMarked
interface TabSwitcherResetHandler {
    /** Reset the tab grid with the given {@link List<Tab>}, which can be null. */
    void resetWithListOfTabs(@Nullable List<Tab> tabs);
}
