// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.app.Activity;
import android.os.Build;

import org.chromium.base.annotations.VerifiesOnN;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelFilterFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabPersistencePolicy;
import org.chromium.chrome.browser.tabmodel.TabbedModeTabPersistencePolicy;

/**
 * Default {@link TabModelSelectorFactory} for Chrome.
 */
public class DefaultTabModelSelectorFactory implements TabModelSelectorFactory {
    // Do not inline since this uses some APIs only available on Android N versions, which cause
    // verification errors.
    @VerifiesOnN
    @Override
    public TabModelSelector buildSelector(Activity activity, TabCreatorManager tabCreatorManager,
            NextTabPolicySupplier nextTabPolicySupplier, int selectorIndex) {
        // Merge tabs if this TabModelSelector is for a ChromeTabbedActivity created in
        // fullscreen mode and there are no TabModelSelector's currently alive. This indicates
        // that it is a cold start or process restart in fullscreen mode.
        boolean mergeTabs = Build.VERSION.SDK_INT > Build.VERSION_CODES.M
                && MultiInstanceManager.isTabModelMergingEnabled()
                && !activity.isInMultiWindowMode();
        if (MultiInstanceManager.shouldMergeOnStartup(activity)) {
            mergeTabs = mergeTabs
                    && (!MultiWindowUtils.getInstance().isInMultiDisplayMode(activity)
                            || TabWindowManagerSingleton.getInstance()
                                            .getNumberOfAssignedTabModelSelectors()
                                    == 0);
        } else {
            mergeTabs = mergeTabs
                    && TabWindowManagerSingleton.getInstance()
                                    .getNumberOfAssignedTabModelSelectors()
                            == 0;
        }
        if (mergeTabs) {
            MultiInstanceManager.mergedOnStartup();
        }
        TabPersistencePolicy persistencePolicy =
                new TabbedModeTabPersistencePolicy(selectorIndex, mergeTabs);
        TabModelFilterFactory tabModelFilterFactory = new ChromeTabModelFilterFactory();
        return new TabModelSelectorImpl(activity, /*windowAndroidSupplier=*/null, tabCreatorManager,
                persistencePolicy, tabModelFilterFactory, nextTabPolicySupplier,
                AsyncTabParamsManagerSingleton.getInstance(), true, true, false);
    }
}
