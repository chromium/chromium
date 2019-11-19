// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import android.os.Bundle;

import org.chromium.base.Callback;

/**
 * Abstract base class that helps define handler that provide a single, no-argument action,
 * implemented synchronously.
 */
public abstract class SimpleDirectActionHandler implements DirectActionHandler {
    private final String mActionId;

    public SimpleDirectActionHandler(String actionId) {
        mActionId = actionId;
    }

    @Override
    public final void reportAvailableDirectActions(DirectActionReporter reporter) {
        if (isAvailable()) reporter.addDirectAction(mActionId);
    }

    @Override
    public final boolean performDirectAction(
            String actionId, Bundle arguments, Callback<Bundle> callback) {
        if (!mActionId.equals(actionId)) return false;

        if (!isAvailable()) return false;

        run();
        callback.onResult(Bundle.EMPTY);
        return true;
    }

    /** Returns {@code true} if the action is currently available. */
    protected abstract boolean isAvailable();

    /**
     * Performs the action, if it is available.
     *
     * <p>{@link #isAvailable} is guaranteed to be called just before this method.
     */
    protected abstract void run();
}
