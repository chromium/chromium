// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.Callback;

/**
 * Interface to send backend requests to a downstream implementation to fulfill password store
 * jobs. All methods are expected to respond asynchronously to callbacks.
 */
public interface PasswordStoreAndroidBackend {
    /**
     * Triggers an async list call to retrieve all logins.
     *
     * @param loginsReply Callback that is called on success with serialized {@link
     *         org.chromium.components.sync.protocol.ListPasswordsResult} data.
     * @param failureCallback A callback that is called on failure for any reason. May return sync.
     */
    void getAllLogins(Callback<byte[]> loginsReply, Callback<Exception> failureCallback);
}
