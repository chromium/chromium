// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.content.Context;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorFactory;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;

/** Default {@link TabModelSelectorFactory} for Chrome. */
public class DefaultTabModelSelectorFactory implements TabModelSelectorFactory {
    // Do not inline since this uses some APIs only available on Android N versions, which cause
    // verification errors.
    @Override
    public TabModelSelector buildSelector(
            Context context,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            TabCreatorManager tabCreatorManager,
            NextTabPolicySupplier nextTabPolicySupplier) {
        AsyncTabParamsManager asyncTabParamsManager = AsyncTabParamsManagerSingleton.getInstance();

        return new TabModelSelectorImpl(
                profileProviderSupplier,
                tabCreatorManager,
                nextTabPolicySupplier,
                asyncTabParamsManager,
                true,
                ActivityType.TABBED,
                false);
    }
}
