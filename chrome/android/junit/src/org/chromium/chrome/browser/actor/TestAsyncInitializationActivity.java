// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.profiles.ProfileProvider;

/**
 * Minimal concrete {@link AsyncInitializationActivity}, since production code checks for that type.
 * Use via {@code Robolectric.buildActivity(...).get()} without driving the lifecycle, since
 * onCreate() requires native.
 */
class TestAsyncInitializationActivity extends AsyncInitializationActivity {
    private boolean mDestroyedForTesting;

    @Override
    public boolean shouldStartGpuProcess() {
        return false;
    }

    @Override
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        return new OneshotSupplierImpl<>();
    }

    @Override
    protected void triggerLayoutInflation() {}

    // Can't drive the real lifecycle: onCreate() needs native, and destroying an uncreated
    // activity trips an ApplicationStatus assert.
    @Override
    public boolean isDestroyed() {
        return mDestroyedForTesting || super.isDestroyed();
    }

    void setDestroyedForTesting(boolean destroyed) {
        mDestroyedForTesting = destroyed;
    }
}
