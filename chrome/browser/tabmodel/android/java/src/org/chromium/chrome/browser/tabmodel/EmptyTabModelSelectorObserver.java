// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;

/**
 * Empty implementation of the tab model selector observer.
 */
public class EmptyTabModelSelectorObserver implements TabModelSelectorObserver {
    @Override
    public void onChange() {}

    @Override
    public void onNewTabCreated(Tab tab, @TabCreationState int creationState) {}

    @Override
    public void onTabModelSelected(TabModel newModel, TabModel oldModel) {}

    @Override
    public void onTabStateInitialized() {}

    @Override
    public void onTabHidden(Tab tab) {}
}
