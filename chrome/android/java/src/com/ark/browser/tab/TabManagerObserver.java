// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser.tab;


import com.ark.browser.tab.core.ITab;
import com.ark.browser.tab.core.ITabGroup;

/**
 * Observes changes to the tab model selector.
 */
public interface TabManagerObserver {

    void onChange();

    void onGroupChanged(ITabGroup newGroup, ITabGroup oldGroup);

    void onTabSelected(ITab tab);

    void onTabMoved(ITab tab, ITabGroup oldGroup);

}
