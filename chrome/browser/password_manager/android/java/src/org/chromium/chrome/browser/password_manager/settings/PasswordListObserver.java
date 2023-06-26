// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.settings;

/**
 * An interface which a client can use to listen to changes to password and password exception
 * lists.
 */
public interface PasswordListObserver {
    /**
     * Called when passwords list is updated.
     *
     * @param count Number of entries in the password list.
     */
    void passwordListAvailable(int count);

    /**
     * Called when password exceptions list is updated.
     *
     * @param count Number of entries in the password exception list.
     */
    void passwordExceptionListAvailable(int count);
}
