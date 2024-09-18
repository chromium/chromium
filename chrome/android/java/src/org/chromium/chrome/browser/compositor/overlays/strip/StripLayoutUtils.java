// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;

public class StripLayoutUtils {
    static final int ANIM_TAB_SLIDE_OUT_MS = 250;
    static final float REORDER_OVERLAP_SWITCH_PERCENTAGE = 0.53f;

    static boolean notRelatedAndEitherTabInGroup(
            TabGroupModelFilter modelFilter, @NonNull Tab tab1, @NonNull Tab tab2) {
        return tab1.getRootId() != tab2.getRootId()
                && (modelFilter.isTabInTabGroup(tab1) || modelFilter.isTabInTabGroup(tab2));
    }
}
