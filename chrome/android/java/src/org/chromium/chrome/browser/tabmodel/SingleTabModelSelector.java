// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.app.Activity;

import org.chromium.chrome.browser.tab.Tab;

/**
 * Simple TabModelSelector that assumes that only a single TabModel exists at a time.
 */
public class SingleTabModelSelector extends TabModelSelectorBase {
    public SingleTabModelSelector(
            Activity activity, TabCreatorManager tabCreatorManager, boolean incognito) {
        super(tabCreatorManager, incognito);
        initialize(new SingleTabModel(activity, incognito));

        TabModelObserver tabModelObserver = new EmptyTabModelObserver() {
            @Override
            public void didCloseTab(int tabId, boolean incognito) {
                // TabModelSelectorImpl handles the equivalent case of closing the last tab in
                // TabModelSelectorImpl#requestToShowTab, which we don't have for this
                // TabModelSelector, so we do it here instead.
                if (getCurrentModel().getTabAt(0) == null) notifyChanged();
            }
        };
        for (TabModel model : getModels()) {
            model.addObserver(tabModelObserver);
        }
    }

    public void setTab(Tab tab) {
        ((SingleTabModel) getCurrentModel()).setTab(tab);
        markTabStateInitialized();
    }
}
