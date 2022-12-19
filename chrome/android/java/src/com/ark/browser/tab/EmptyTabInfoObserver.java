// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser.tab;

import com.ark.browser.tab.core.IPage;
import com.ark.browser.tab.core.ITab;

import org.chromium.chrome.browser.tab.TabSelectionType;

/**
 * An empty base implementation of the TabModelObserver interface.
 */
public class EmptyTabInfoObserver implements TabInfoObserver {
    @Override
    public void didSelectTab(ITab tab, @TabSelectionType int type, int lastId) {}

    @Override
    public void didCloseTab(int tabId, boolean incognito) {}

    @Override
    public void didAddTab(ITab tab, @TabSelectionType int type) {}

}
