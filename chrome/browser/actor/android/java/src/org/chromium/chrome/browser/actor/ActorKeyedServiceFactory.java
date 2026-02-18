// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import androidx.annotation.Nullable;

import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/** Standard Chromium pattern for accessing ActorKeyedService in Java. */
@NullMarked
public class ActorKeyedServiceFactory {
    @Nullable private static ActorKeyedService sServiceForTesting;

    private ActorKeyedServiceFactory() {}

    /**
     * Retrieves the service instance for a given profile. Returns null if the profile is
     * off-the-record or the service is not available.
     */
    @Nullable
    public static ActorKeyedService getForProfile(Profile profile) {
        if (sServiceForTesting != null) return sServiceForTesting;
        // JNI call will return null if the profile is off-the-record or service not available.
        return ActorKeyedServiceFactoryJni.get().getForProfile(profile);
    }

    public static void setForTesting(@Nullable ActorKeyedService service) {
        sServiceForTesting = service;
        ResettersForTesting.register(() -> sServiceForTesting = null);
    }

    @NativeMethods
    interface Natives {
        ActorKeyedService getForProfile(Profile profile);
    }
}
