// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.base.ThreadUtils.assertOnUiThread;

/**
 * This factory returns an implementation for the password settings accessor. The factory itself is
 * also implemented downstream.
 */
public abstract class PasswordSettingsAccessorFactory {
    private static PasswordSettingsAccessorFactory sInstance;

    /**
     * Returns a settings accessor factory to be invoked whenever {@link #createAccessor()} is
     * called. If no factory was used yet, it is created.
     *
     * @return The shared {@link PasswordSettingsAccessorFactory} instance.
     */
    public static PasswordSettingsAccessorFactory getInstance() {
        assertOnUiThread();
        // TODO(crbug.com/1289700): Create an instance if it doesn't exist and return it.
        return sInstance;
    }

    /**
     * Returns the downstream implementation provided by subclasses.
     *
     * @return An implementation of the {@link PasswordSettingsAccessor} if one exists.
     */
    public PasswordSettingsAccessor createAccessor() {
        return null;
    }
}
