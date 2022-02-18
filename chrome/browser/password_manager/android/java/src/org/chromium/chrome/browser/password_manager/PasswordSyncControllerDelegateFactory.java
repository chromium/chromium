// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

/**
 * This factory returns an implementation for the {@link PasswordSyncControllerDelegate}.
 * The factory itself is implemented downstream, too.
 */
public abstract class PasswordSyncControllerDelegateFactory {
    /**
     * Returns the downstream implementation provided by subclasses.
     *
     * @return An implementation of the {@link PasswordSyncControllerDelegate}. May be
     * null for builds without a downstream delegate implementation.
     */
    public PasswordSyncControllerDelegate createDelegate() {
        return null;
    }
}
