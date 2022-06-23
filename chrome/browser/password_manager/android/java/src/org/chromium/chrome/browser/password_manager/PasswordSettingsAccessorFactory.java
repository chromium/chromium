// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

/**
 * This factory returns an implementation for the password settings accessor. The factory itself is
 * also implemented downstream.
 */
public abstract class PasswordSettingsAccessorFactory {
    /**
     * Returns the downstream implementation provided by subclasses.
     *
     * @return An implementation of the {@link PasswordSettingsAccessor} if one exists.
     */
    public PasswordSettingsAccessor createAccessor() {
        return null;
    }

    public boolean canCreateAccessor() {
        return false;
    }
}
