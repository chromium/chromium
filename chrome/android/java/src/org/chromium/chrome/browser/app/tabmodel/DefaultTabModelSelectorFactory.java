// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import android.content.Context;
import android.util.Pair;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabwindow.TabModelSelectorFactory;
import org.chromium.chrome.browser.tabwindow.WindowId;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Default {@link TabModelSelectorFactory} for Chrome. */
@NullMarked
public class DefaultTabModelSelectorFactory implements TabModelSelectorFactory {
    // Do not inline since this uses some APIs only available on Android N versions, which cause
    // verification errors.
    @Override
    public TabModelSelector buildTabbedSelector(
            Context context,
            ModalDialogManager modalDialogManager,
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            TabCreatorManager tabCreatorManager,
            NextTabPolicySupplier nextTabPolicySupplier,
            MultiInstanceManager multiInstanceManager) {
        AsyncTabParamsManager asyncTabParamsManager = AsyncTabParamsManagerSingleton.getInstance();

        return new TabModelSelectorImpl(
                context,
                modalDialogManager,
                profileProviderSupplier,
                tabCreatorManager,
                nextTabPolicySupplier,
                multiInstanceManager,
                asyncTabParamsManager,
                true,
                ActivityType.TABBED,
                false);
    }

    @Override
    public Pair<TabModelSelector, Destroyable> buildHeadlessSelector(
            @WindowId int windowId, Profile profile) {
        HeadlessTabModelOrchestrator orchestrator =
                new HeadlessTabModelOrchestrator(windowId, profile);
        return Pair.create(orchestrator.getTabModelSelector(), orchestrator);
    }
}
