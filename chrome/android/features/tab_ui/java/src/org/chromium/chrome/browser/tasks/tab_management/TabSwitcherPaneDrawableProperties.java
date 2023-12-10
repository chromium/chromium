// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/** Properties for the {@link TabSwitcherDrawable} for the {@link TabSwitcherPane}. */
public class TabSwitcherPaneDrawableProperties {
    public static final WritableIntPropertyKey TAB_COUNT = new WritableIntPropertyKey();

    public static final PropertyKey[] ALL_KEYS = new PropertyKey[] {TAB_COUNT};
}
