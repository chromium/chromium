// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.List;

/**
 * Helper class to simplify tracking and unregistering {@link HubViewColorBlend}s for view-scoped
 * components.
 */
@NullMarked
public class HubColorMixerRegistrationHelper {
    private final List<HubViewColorBlend> mRegisteredBlends = new ArrayList<>();
    private @Nullable HubColorMixer mColorMixer;

    /**
     * Sets the {@link HubColorMixer} to register/unregister blends on.
     *
     * @param mixer The new color mixer, or null to unregister all blends.
     */
    public void setColorMixer(@Nullable HubColorMixer mixer) {
        if (mColorMixer == mixer) return;
        if (mColorMixer != null) {
            for (HubViewColorBlend blend : mRegisteredBlends) {
                mColorMixer.unregisterBlend(blend);
            }
        }
        mColorMixer = mixer;
        if (mColorMixer != null) {
            for (HubViewColorBlend blend : mRegisteredBlends) {
                mColorMixer.registerBlend(blend);
            }
        }
    }

    /**
     * Registers a {@link HubViewColorBlend} to the helper. It will be registered to the current
     * {@link HubColorMixer} if one is set.
     *
     * @param blend The blend to register.
     */
    public void registerBlend(HubViewColorBlend blend) {
        if (mRegisteredBlends.contains(blend)) {
            return;
        }
        mRegisteredBlends.add(blend);
        if (mColorMixer != null) {
            mColorMixer.registerBlend(blend);
        }
    }

    /** Unregisters all blends and clears the color mixer. */
    public void destroy() {
        if (mColorMixer != null) {
            for (HubViewColorBlend blend : mRegisteredBlends) {
                mColorMixer.unregisterBlend(blend);
            }
        }
        mRegisteredBlends.clear();
        mColorMixer = null;
    }
}
