// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import org.chromium.base.Callback;

/** Defines an interface for installing WebAPKs via Google Play. */
public interface GooglePlayWebApkInstallDelegate {
    /**
     * Uses Google Play to install WebAPK asynchronously.
     * @param packageName The package name of WebAPK to install.
     * @param version The version of WebAPK to install.
     * @param title The title of the WebAPK to display during installation.
     * @param token The token from WebAPK Minter Server.
     * @param callback The callback to invoke when the install completes, times out or fails.
     */
    void installAsync(
            String packageName,
            int version,
            String title,
            String token,
            Callback<Integer> callback);

    /**
     * Uses Google Play to update WebAPK asynchronously.
     * @param packageName The package name of WebAPK to update.
     * @param version The version of WebAPK to update.
     * @param title The title of the WebAPK to display during update.
     * @param token The token from WebAPK Minter Server.
     * @param callback The callback to invoke when the update completes, times out or fails.
     */
    void updateAsync(
            String packageName,
            int version,
            String title,
            String token,
            Callback<Integer> callback);
}
