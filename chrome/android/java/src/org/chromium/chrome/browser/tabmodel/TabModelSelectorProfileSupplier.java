// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * {@link MonotonicObservableSupplier} for {@link Profile} that updates each time the profile of the current
 * tab model changes, e.g. if the current tab model switches to/from incognito. Like {@link
 * MonotonicObservableSupplier}, this class must only be accessed from a single
 * thread.
 */
@NullMarked
public class TabModelSelectorProfileSupplier
        implements MonotonicObservableSupplier<Profile>, Destroyable {
    private final TabModelSelectorObserver mSelectorObserver;
    private final MonotonicObservableSupplier<TabModelSelector> mSelectorSupplier;
    private final Callback<TabModelSelector> mSelectorSupplierCallback;
    private final Callback<TabModel> mCurrentTabModelObserver;
    private final SettableMonotonicObservableSupplier<Profile> mSupplier =
            ObservableSuppliers.createMonotonic();

    private @Nullable TabModelSelector mSelector;

    public TabModelSelectorProfileSupplier(
            MonotonicObservableSupplier<TabModelSelector> selectorSupplier) {
        mSelectorObserver =
                new TabModelSelectorObserver() {
                    @Override
                    public void onChange() {
                        assumeNonNull(mSelector);
                        if (mSelector.getCurrentModel() == null) return;
                        Profile profile = mSelector.getCurrentModel().getProfile();
                        if (profile == null) return;
                        set(profile);
                    }

                    @Override
                    public void onTabStateInitialized() {
                        assumeNonNull(mSelector);
                        Profile profile = mSelector.getCurrentModel().getProfile();
                        if (profile == null) return;
                        set(profile);
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
        mSelectorSupplier.addSyncObserverAndPostIfNonNull(mSelectorSupplierCallback);

        var selector = mSelectorSupplier.get();
        if (selector != null) {
            setSelector(selector);
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
        mSelector
                .getCurrentTabModelSupplier()
                .addSyncObserverAndPostIfNonNull(mCurrentTabModelObserver);

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

    private void set(@Nullable Profile profile) {
        assert profile != null : "Cannot set a null Profile";
        // TODO(365814339): Convert to checked exception once all callsites are fixed.
        assert !profile.shutdownStarted() : "Attempting to set an already destroyed Profile";
        mSupplier.set(profile);
    }

    @Override
    public @Nullable Profile get() {
        Profile profile = mSupplier.get();
        // TODO(365814339): Convert to checked exception once all callsites are fixed.
        assert profile == null || !profile.shutdownStarted()
                : "Attempting to access an already destroyed Profile";
        return profile;
    }

    @Override
    public @Nullable Profile addObserver(Callback<Profile> obs, int behavior) {
        return mSupplier.addObserver(obs, behavior);
    }

    @Override
    public void removeObserver(Callback<Profile> obs) {
        mSupplier.removeObserver(obs);
    }

    @Override
    public int getObserverCount() {
        return mSupplier.getObserverCount();
    }

    @Override
    public NonNullObservableSupplier<Profile> asNonNull() {
        return mSupplier.asNonNull();
    }
}
