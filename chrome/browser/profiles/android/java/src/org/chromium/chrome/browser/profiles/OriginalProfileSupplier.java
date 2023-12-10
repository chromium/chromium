// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.OneshotSupplierImpl;

/**
 * A {@link OneshotSupplierImpl} for the first, non-incognito, {@link Profile} created.
 *
 * <p>This class notifies observers when the original {@link Profile} object is first created.
 * Creation happens once per process on Android; all other profiles are incognito derivatives of the
 * original profile.
 *
 * <p>If you call {@link #onAvailable(org.chromium.base.Callback)} after the original profile has
 * been created, a task will be immediately posted to notify your observer, and the profile will be
 * synchronously returned. Like {@link org.chromium.base.supplier.OneshotSupplier}, this class must
 * only be accessed from a single thread.
 */
public class OriginalProfileSupplier extends OneshotSupplierImpl<Profile>
        implements Destroyable, ProfileManager.Observer {
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

    @Override
    public void destroy() {
        if (!hasValue()) ProfileManager.removeObserver(this);
    }
}
