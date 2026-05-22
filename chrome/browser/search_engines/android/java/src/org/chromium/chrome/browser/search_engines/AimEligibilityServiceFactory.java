// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/** Factory for eligibility checks for the AimEligibilityService. */
@NullMarked
public class AimEligibilityServiceFactory {
    private AimEligibilityServiceFactory() {}

    /** Returns whether the Aim starter pack is enabled for the given profile. */
    public static boolean isAimStarterPackEnabled(Profile profile) {
        ThreadUtils.assertOnUiThread();
        return AimEligibilityServiceFactoryJni.get().isAimStarterPackEnabled(profile);
    }

    @NativeMethods
    public interface Natives {
        boolean isAimStarterPackEnabled(@JniType("Profile*") Profile profile);
    }
}
