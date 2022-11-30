// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_check;

import org.chromium.base.Callback;

import java.lang.ref.WeakReference;

/**
 * Interface to interact with the version updater.
 *
 * Because the current {@link OmahaBase} implementation is in //chrome/android,
 * it is necessary to have some glue code in chrome_java that implements this
 * interface. Once Omaha is componentized, this pattern will no longer be
 * needed.
 */
public interface SafetyCheckUpdatesDelegate {
    /**
     * Asynchronously checks for updates and invokes the provided callback with
     * the result.
     * @param statusCallback A callback to invoke with the result. Takes an element of
     *                       {@link SafetyCheckProperties.UpdatesState} as an argument.
     */
    void checkForUpdates(WeakReference<Callback<Integer>> statusCallback);
}
