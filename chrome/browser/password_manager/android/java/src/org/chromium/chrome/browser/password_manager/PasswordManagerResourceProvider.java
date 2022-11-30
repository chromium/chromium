// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import androidx.annotation.DrawableRes;

/**
 * Provides functions that choose the correct resource IDs for the password manager UI.
 * Upstream resources may be overridden by a downstream implementation.
 */
public interface PasswordManagerResourceProvider {
    /**
     * Returns the drawable id to be displayed as a password manager key icon.
     *
     * @return A resource file for the 24dp logo.
     */
    @DrawableRes
    int getPasswordManagerIcon();
}
