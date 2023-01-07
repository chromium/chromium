// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.app.Activity;

import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelFilterFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;

/**
 * Default {@link TabModelSelectorFactory} for Chrome.
 */
public class DefaultTabModelSelectorFactory implements TabModelSelectorFactory {
    // Do not inline since this uses some APIs only available on Android N versions, which cause
    // verification errors.
    @Override
    public TabModelSelector buildSelector(Activity activity, TabCreatorManager tabCreatorManager,
            NextTabPolicySupplier nextTabPolicySupplier, int selectorIndex) {
        TabModelFilterFactory tabModelFilterFactory = new ChromeTabModelFilterFactory(activity);
        AsyncTabParamsManager asyncTabParamsManager = AsyncTabParamsManagerSingleton.getInstance();

        return new TabModelSelectorImpl(/*windowAndroidSupplier=*/null, tabCreatorManager,
                tabModelFilterFactory, nextTabPolicySupplier, asyncTabParamsManager, true,
                ActivityType.TABBED, false);
    }
}
