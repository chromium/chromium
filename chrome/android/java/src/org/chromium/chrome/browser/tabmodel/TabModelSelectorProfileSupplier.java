// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.lifetime.Destroyable;
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
        extends ObservableSupplierImpl<Profile> implements TabModelSelectorObserver, Destroyable {
    private TabModelSelector mSelector;
    private boolean mIsTabStateInitialized;

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
        assert !mIsTabStateInitialized || newProfile != null;

        // Postpone setting the profile until tab state is initialized.
        if (newProfile == null) return;
        set(newProfile);
    }

    @Override
    public void onChange() {}

    @Override
    public void onNewTabCreated(Tab tab, int creationState) {}

    @Override
    public void onTabHidden(Tab tab) {}

    @Override
    public void onTabStateInitialized() {
        mIsTabStateInitialized = true;
        Profile profile = mSelector.getCurrentModel().getProfile();
        assert profile != null;
        set(profile);
    }

    @Override
    public void destroy() {
        if (mSelector != null) {
            mSelector.removeObserver(this);
            mSelector = null;
        }
    }
}
