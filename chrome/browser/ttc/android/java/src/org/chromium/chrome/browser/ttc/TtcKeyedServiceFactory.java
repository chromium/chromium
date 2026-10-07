// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/** Provides access to the {@link TtcKeyedService} for a profile. */
@JNINamespace("ttc")
@NullMarked
public class TtcKeyedServiceFactory {
    private static @Nullable TtcKeyedService sServiceForTesting;

    private TtcKeyedServiceFactory() {}

    /**
     * Returns the service for {@code profile}, or null if TTC is not available for it (e.g. the
     * feature is disabled or the profile is off-the-record). Repeated calls return the same Java
     * object: the native bridge holds it in a global ref for the service's lifetime.
     */
    public static @Nullable TtcKeyedService getForProfile(Profile profile) {
        if (sServiceForTesting != null) return sServiceForTesting;
        return TtcKeyedServiceFactoryJni.get().getForProfile(profile);
    }

    public static void setForTesting(@Nullable TtcKeyedService service) {
        sServiceForTesting = service;
        ResettersForTesting.register(() -> sServiceForTesting = null);
    }

    @NativeMethods
    interface Natives {
        @Nullable TtcKeyedService getForProfile(@JniType("Profile*") Profile profile);
    }
}
