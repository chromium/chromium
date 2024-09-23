// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_check;

import androidx.annotation.Nullable;

import org.chromium.base.ResettersForTesting;

/**
 * Use {@link #getOrCreate()} to instantiate a {@link PasswordCheckImpl} and {@link #destroy()} when
 * the instance is no longer needed.
 */
public class PasswordCheckFactory {
    private static PasswordCheck sPasswordCheck;

    private PasswordCheckFactory() {}

    /**
     * Creates a {@link PasswordCheckImpl} if none exists. Otherwise it returns the existing
     * instance.
     *
     * @return A {@link PasswordCheckImpl} or null if the feature is disabled.
     */
    public static @Nullable PasswordCheck getOrCreate() {
        if (sPasswordCheck == null) {
            sPasswordCheck = new PasswordCheckImpl();
        }
        return sPasswordCheck;
    }

    /**
     * Destroys the C++ layer to free up memory. In order to not leave a partially initialized
     * feature component alive, it sets the {@link PasswordCheckImpl} to null.
     *
     * Should be called by the last object alive who needs the feature. This is, in general,
     * the outermost settings screen. Note that this is not always {@link MainSettings}.
     */
    public static void destroy() {
        if (sPasswordCheck == null) return;
        sPasswordCheck.destroy();
        sPasswordCheck = null;
    }

    public static void setPasswordCheckForTesting(PasswordCheck passwordCheck) {
        var oldValue = sPasswordCheck;
        sPasswordCheck = passwordCheck;
        ResettersForTesting.register(() -> sPasswordCheck = oldValue);
    }

    /**
     * Returns the underlying instance.
     * Should only be used when there's a need to avoid creating a new instance.
     * @return A {@link PasswordCheeck} instance as stored here.
     */
    public static PasswordCheck getPasswordCheckInstance() {
        return sPasswordCheck;
    }
}
