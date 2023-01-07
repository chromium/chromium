// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.Callback;

/**
 * Interface to notify CredentialManager about sync events. All methods are expected to respond
 * asynchronously to callbacks.
 */
public interface PasswordSyncControllerDelegate {
    /**
     * Triggers an async call to notify credential manager of the currently syncing account.
     *
     * @param accountName Name of the sync account.
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason.
     */
    void notifyCredentialManagerWhenSyncing(
            String accountName, Runnable successCallback, Callback<Exception> failureCallback);

    /**
     * Triggers an async call to notify credential manager of a local account, when not syncing.
     *
     * @param successCallback Callback that is called on success.
     * @param failureCallback A callback that is called on failure for any reason.
     */
    void notifyCredentialManagerWhenNotSyncing(
            Runnable successCallback, Callback<Exception> failureCallback);
}
