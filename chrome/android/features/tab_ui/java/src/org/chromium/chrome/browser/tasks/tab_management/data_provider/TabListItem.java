// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.data_provider;

import org.chromium.build.annotations.NullMarked;

/** Polymorphic item contract representing a visual row or card entry in a tab list surface. */
@NullMarked
public interface TabListItem {
    /** Returns whether this item is currently selected. */
    boolean isSelected();

    /** Returns a copy of this item with the specified selection state. */
    TabListItem withSelected(boolean isSelected);
}
