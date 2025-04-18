// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;

/** {@link TabModelSelector} for interacting with tabs without an activity. */
public class HeadlessTabModelSelectorImpl extends TabModelSelectorImpl {

    private static OneshotSupplierImpl<ProfileProvider> wrapProfile(Profile profile) {
        ProfileProvider profileProvider =
                new ProfileProvider() {
                    @Override
                    public Profile getOriginalProfile() {
                        return profile;
                    }

                    @Override
                    public @Nullable Profile getOffTheRecordProfile(boolean createIfNeeded) {
                        return profile.getPrimaryOtrProfile(createIfNeeded);
                    }

                    @Override
                    public boolean hasOffTheRecordProfile() {
                        return profile.hasPrimaryOtrProfile();
                    }
                };
        OneshotSupplierImpl<ProfileProvider> ourProfileProviderSupplier =
                new OneshotSupplierImpl<>();
        ourProfileProviderSupplier.set(profileProvider);
        return ourProfileProviderSupplier;
    }

    public HeadlessTabModelSelectorImpl(Profile profile, TabCreatorManager tabCreatorManager) {
        super(
                ContextUtils.getApplicationContext(),
                /* modalDialogManager= */ null,
                wrapProfile(profile),
                tabCreatorManager,
                () -> NextTabPolicy.LOCATIONAL,
                AsyncTabParamsManagerFactory.createAsyncTabParamsManager(),
                /* supportUndo= */ false,
                ActivityType.TABBED,
                /* startIncognito= */ false);
    }

    @Override
    public void requestToShowTab(Tab tab, @TabSelectionType int type) {
        // Intentional noop.
    }
}
