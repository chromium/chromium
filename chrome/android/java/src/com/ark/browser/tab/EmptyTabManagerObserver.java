// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser.tab;

import com.ark.browser.tab.core.ITab;
import com.ark.browser.tab.core.ITabGroup;

/**
 * Empty implementation of the tab model selector observer.
 */
public class EmptyTabManagerObserver implements TabManagerObserver {

    @Override
    public void onChange() {}

    @Override
    public void onTabMoved(ITab tab, ITabGroup oldGroup) {}
}
