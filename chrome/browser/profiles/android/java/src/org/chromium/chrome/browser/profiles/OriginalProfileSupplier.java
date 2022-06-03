// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import org.chromium.base.supplier.ObservableSupplierImpl;

/**
 * {@link org.chromium.base.supplier.ObservableSupplier} for {@link Profile} that notifies observers
 * when the original {@link Profile} object is first created. Creation happens once per process on
 * Android; all other profiles are incognito derivatives of the original profile.
 *
 * <p>
 * If you call addObserver after the original profile has been created, a task
 * will be immediately posted to notify your observer, and the profile will be synchronously
 * returned. Like {@link org.chromium.base.supplier.ObservableSupplier}, this class must only be
 * accessed from a single thread.
 */
public class OriginalProfileSupplier
        extends ObservableSupplierImpl<Profile> implements ProfileManager.Observer {
    public OriginalProfileSupplier() {
        ProfileManager.addObserver(this);
    }

    @Override
    public void onProfileAdded(Profile profile) {
        if (profile.isOffTheRecord()) return;
        set(profile);
        ProfileManager.removeObserver(this);
    }

    @Override
    public void onProfileDestroyed(Profile profile) {}
}
