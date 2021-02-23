// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabImpl;
import org.chromium.chrome.browser.tabmodel.TabModel;

import java.util.ArrayList;
import java.util.List;

/**
 * Creates the c++ class that provides ChromeAutocompleteProviderClient to access java resources.
 */
public class ChromeAutocompleteProviderClient {
    @CalledByNative
    private static TabImpl[] getAllHiddenAndNonCCTTabs(TabModel[] tabModels) {
        if (tabModels == null) return null;
        List<Tab> tabList = new ArrayList<Tab>();

        for (TabModel tabModel : tabModels) {
            if (tabModel == null) continue;
            for (int i = 0; i < tabModel.getCount(); ++i) {
                TabImpl tab = (TabImpl) tabModel.getTabAt(i);
                if (tab.isHidden() && !tab.isCustomTab()) {
                    tabList.add(tab);
                }
            }
        }
        if (tabList.size() == 0) {
            return null;
        }

        TabImpl[] tabImplArray = new TabImpl[tabList.size()];
        for (int i = 0; i < tabList.size(); i++) {
            tabImplArray[i] = (TabImpl) tabList.get(i);
        }
        return tabImplArray;
    }
}
