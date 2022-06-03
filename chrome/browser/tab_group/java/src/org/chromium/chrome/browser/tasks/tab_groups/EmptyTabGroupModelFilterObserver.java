// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_groups;

import org.chromium.chrome.browser.tab.Tab;

import java.util.List;

/**
 * An empty implementation of {@link TabGroupModelFilter.Observer}.
 */
public class EmptyTabGroupModelFilterObserver implements TabGroupModelFilter.Observer {
    @Override
    public void willMergeTabToGroup(Tab movedTab, int newRootId) {}

    @Override
    public void willMoveTabOutOfGroup(Tab movedTab, int newRootId) {}

    @Override
    public void didMergeTabToGroup(Tab movedTab, int selectedTabIdInGroup) {}

    @Override
    public void didMoveTabGroup(Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {}

    @Override
    public void didMoveWithinGroup(Tab movedTab, int tabModelOldIndex, int tabModelNewIndex) {}

    @Override
    public void didMoveTabOutOfGroup(Tab movedTab, int prevFilterIndex) {}

    @Override
    public void didCreateGroup(
            List<Tab> tabs, List<Integer> tabOriginalIndex, boolean isSameGroup) {}
}
