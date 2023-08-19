// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
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
        extends ObservableSupplierImpl<Profile> implements Destroyable {
    private final TabModelSelectorObserver mSelectorObserver;
    private final ObservableSupplier<TabModelSelector> mSelectorSupplier;
    private final Callback<TabModelSelector> mSelectorSupplierCallback;

    private TabModelSelector mSelector;
    private boolean mHasProfile;

    public TabModelSelectorProfileSupplier(ObservableSupplier<TabModelSelector> selectorSupplier) {
        mSelectorObserver = new TabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                Profile newProfile = newModel.getProfile();
                // Postpone setting the profile until tab state is initialized.
                if (newProfile == null) return;
                set(newProfile);
            }

            @Override
            public void onChange() {
                if (mSelector.getCurrentModel() == null) return;
                Profile profile = mSelector.getCurrentModel().getProfile();
                if (profile == null) return;
                set(profile);
            }

            @Override
            public void onNewTabCreated(Tab tab, int creationState) {}

            @Override
            public void onTabHidden(Tab tab) {}

            @Override
            public void onTabStateInitialized() {
                set(mSelector.getCurrentModel().getProfile());
            }
        };

        mSelectorSupplier = selectorSupplier;
        mSelectorSupplierCallback = this::setSelector;
        mSelectorSupplier.addObserver(mSelectorSupplierCallback);

        if (mSelectorSupplier.hasValue()) {
            setSelector(mSelectorSupplier.get());
        }
    }

    private void setSelector(TabModelSelector selector) {
        if (mSelector == selector) return;
        if (mSelector != null) mSelector.removeObserver(mSelectorObserver);

        mSelector = selector;
        mSelector.addObserver(mSelectorObserver);

        if (selector.getCurrentModel() != null) {
            mSelectorObserver.onTabModelSelected(selector.getCurrentModel(), null);
        }
    }

    @Override
    public void destroy() {
        if (mSelector != null) {
            mSelector.removeObserver(mSelectorObserver);
            mSelector = null;
        }
        mSelectorSupplier.removeObserver(mSelectorSupplierCallback);
    }

    @Override
    public void set(Profile profile) {
        if (profile == null) {
            throw new IllegalStateException("Null is not a valid value to set for the profile.");
        }
        mHasProfile = true;
        super.set(profile);
    }

    @Nullable
    @Override
    public Profile get() {
        Profile profile = super.get();
        // TODO(crbug.com/1353138): Convert to an IllegalStateException if no bug reports are filed.
        assert profile
                != null : ("Attempting to read a null profile from the supplier. Use "
                                  + "hasValue() instead and add an observer.");
        return profile;
    }

    @Override
    public boolean hasValue() {
        return mHasProfile;
    }
}
