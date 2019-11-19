// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import android.os.Bundle;

import org.chromium.base.Callback;

/**
 * Implements the reporting and handling of a set of direct actions.
 *
 * Handlers available for the current activity should be registered to that activity's {@link
 * DirectActionCoordinator}.
 */
public interface DirectActionHandler {
    /** Reports currently available actions. */
    void reportAvailableDirectActions(DirectActionReporter reporter);

    /**
     * Perform the action identified {@code actionId}, if supported by this handler.
     *
     * @param actionId The action identifier
     * @param arguments Argument provided by the caller
     * @param callback Callback to report the result of the action to
     * @return true if the action was handled by this handler.
     */
    boolean performDirectAction(String actionId, Bundle arguments, Callback<Bundle> callback);
}
