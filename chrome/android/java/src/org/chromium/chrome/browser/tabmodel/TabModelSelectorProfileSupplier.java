// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

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
public class TabModelSelectorProfileSupplier extends ObservableSupplierImpl<Profile>
        implements Destroyable {
    private final TabModelSelectorObserver mSelectorObserver;
    private final ObservableSupplier<TabModelSelector> mSelectorSupplier;
    private final Callback<TabModelSelector> mSelectorSupplierCallback;
    private final Callback<TabModel> mCurrentTabModelObserver;

    private TabModelSelector mSelector;

    public TabModelSelectorProfileSupplier(ObservableSupplier<TabModelSelector> selectorSupplier) {
        mSelectorObserver =
                new TabModelSelectorObserver() {
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
        mCurrentTabModelObserver =
                (tabModel) -> {
                    Profile newProfile = tabModel.getProfile();
                    // Postpone setting the profile until tab state is initialized.
                    if (newProfile == null) return;
                    set(newProfile);
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
        if (mSelector != null) {
            mSelector.removeObserver(mSelectorObserver);
            mSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
        }

        mSelector = selector;
        mSelector.addObserver(mSelectorObserver);
        mSelector.getCurrentTabModelSupplier().addObserver(mCurrentTabModelObserver);

        if (selector.getCurrentModel() != null) {
            mCurrentTabModelObserver.onResult(selector.getCurrentModel());
        }
    }

    @Override
    public void destroy() {
        if (mSelector != null) {
            mSelector.removeObserver(mSelectorObserver);
            mSelector.getCurrentTabModelSupplier().removeObserver(mCurrentTabModelObserver);
            mSelector = null;
        }
        mSelectorSupplier.removeObserver(mSelectorSupplierCallback);
    }

    @Override
    public void set(Profile profile) {
        if (profile == null) {
            throw new IllegalStateException("Null is not a valid value to set for the profile.");
        }
        // TODO(365814339): Convert to checked exception once all callsites are fixed.
        assert !profile.shutdownStarted() : "Attempting to set an already destroyed Profile";
        super.set(profile);
    }

    @Override
    public Profile get() {
        Profile profile = super.get();
        if (profile == null) {
            // Prevent unintentional access to a null profile early during app initialization. If a
            // client wants to read this when it could be null, use hasValue() and add an observer
            // to be notified when the profile becomes available.
            throw new IllegalStateException("Attempting to read a null profile from the supplier");
        }
        // TODO(365814339): Convert to checked exception once all callsites are fixed.
        assert !profile.shutdownStarted() : "Attempting to access an already destroyed Profile";
        return profile;
    }

    @Override
    public boolean hasValue() {
        // this.get() will throw on null, so go directly to super.
        return super.get() != null;
    }
}
