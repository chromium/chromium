// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ssl;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/** Bridge providing access to native-side HTTPS-First Mode data. */
@NullMarked
public final class HttpsFirstModeBridge {
    private final Profile mProfile;

    /** Constructs a {@link HttpsFirstModeBridge} associated with the given {@link Profile}. */
    public HttpsFirstModeBridge(Profile profile) {
        mProfile = profile;
    }

    /**
     * @return The HTTPS-First Mode setting state. It can be Disabled, Full, or Balanced.
     */
    public @HttpsFirstModeSetting int getCurrentSetting() {
        return HttpsFirstModeBridgeJni.get().getCurrentSetting(mProfile);
    }

    /**
     * Sets the underlying HTTPS-First Mode prefs based on `state`.
     *
     * @param state Current setting state. It can be Disabled, Full, or Balanced.
     */
    public void updatePrefs(@HttpsFirstModeSetting int state) {
        HttpsFirstModeBridgeJni.get().updatePrefs(mProfile, state);
    }

    /**
     * @return Whether the HTTPS-First Mode preference is managed.
     */
    public boolean isManaged() {
        return HttpsFirstModeBridgeJni.get().isManaged(mProfile);
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        @HttpsFirstModeSetting
        int getCurrentSetting(Profile profile);

        void updatePrefs(Profile profile, @HttpsFirstModeSetting int state);

        boolean isManaged(Profile profile);
    }
}
