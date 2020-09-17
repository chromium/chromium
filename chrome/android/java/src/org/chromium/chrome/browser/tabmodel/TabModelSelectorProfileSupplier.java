// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;

/**
 * {@link ObservableSupplier} for {@link Profile} that updates each time the profile of the current
 * tab model changes, e.g. if the current tab model switches to/from incognito.
 * Like {@link org.chromium.base.supplier.ObservableSupplier}, this class must only be
 * accessed from a single thread.
 */
public class TabModelSelectorProfileSupplier
        extends ObservableSupplierImpl<Profile> implements TabModelSelectorObserver {
    private TabModelSelector mSelector;

    public TabModelSelectorProfileSupplier(ObservableSupplier<TabModelSelector> selectorSupplier) {
        selectorSupplier.addObserver(this::setSelector);
    }

    private void setSelector(TabModelSelector selector) {
        mSelector = selector;
        mSelector.addObserver(this);
    }

    @Override
    public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
        Profile newProfile = newModel.getProfile();
        // When switching to an incognito tab model, the corresponding off-the-record profile does
        // not necessarily exist yet, but we may be able to force its creation.
        if (newProfile == null && newModel.isIncognito()) {
            Profile oldProfile = oldModel.getProfile();
            assert oldProfile != null;
            // If the previous profile is itself off-the-record, we can't derive an
            // off-the-record profile from it.
            if (oldProfile.isOffTheRecord()) return;
            // Forces creation of a primary off-the-record profile. TODO(pnoland): replace this with
            // getIncognitoProfile() once multiple OTR profiles are supported on Android.
            newProfile = oldProfile.getOffTheRecordProfile();
        }

        if (newProfile == null) return;

        set(newProfile);
    }

    @Override
    public void onChange() {}

    @Override
    public void onNewTabCreated(Tab tab, int creationState) {}

    @Override
    public void onTabStateInitialized() {
        Profile profile = mSelector.getCurrentModel().getProfile();
        if (profile == null) {
            // Since only IncognitoTabModelImpl provides null profile, the current model should be
            // off-the-record.
            assert mSelector.getCurrentModel().isIncognito();
            // TODO(https://crbug.com/1060940): Update to cover all OTR profiles.
            profile = Profile.getLastUsedRegularProfile().getPrimaryOTRProfile();
        }
        set(profile);
    }

    public void destroy() {
        if (mSelector != null) {
            mSelector.removeObserver(this);
            mSelector = null;
        }
    }
}