// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.tab.Tab;

/**
 * An empty implementation of {@link TabModelFilter} interface. This is a {@link TabList} that
 * contains the same {@link Tab}s as {@link TabModel} does.
 */
public class EmptyTabModelFilter extends TabModelFilter {
    public EmptyTabModelFilter(TabModel tabModel) {
        super(tabModel);
    }

    // TabModelFilter implementation.
    @Override
    protected void addTab(Tab tab) {}

    @Override
    protected void closeTab(Tab tab) {}

    @Override
    protected void selectTab(Tab tab) {}

    @Override
    protected void reorder() {}

    @Override
    protected void resetFilterStateInternal() {}

    @Override
    protected void removeTab(Tab tab) {}

    // TabList implementation.
    @Override
    public boolean isIncognito() {
        return getTabModel().isIncognito();
    }

    @Override
    public int index() {
        return getTabModel().index();
    }

    @Override
    public int getCount() {
        return getTabModel().getCount();
    }

    @Override
    public Tab getTabAt(int index) {
        return getTabModel().getTabAt(index);
    }

    @Override
    public int indexOf(Tab tab) {
        return getTabModel().indexOf(tab);
    }

    @Override
    public boolean isClosurePending(int tabId) {
        return getTabModel().isClosurePending(tabId);
    }
}
