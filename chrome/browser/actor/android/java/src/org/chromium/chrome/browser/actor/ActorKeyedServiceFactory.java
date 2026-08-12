// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.actor;

import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/** Standard Chromium pattern for accessing ActorKeyedService in Java. */
@NullMarked
public class ActorKeyedServiceFactory {
    @Nullable private static ActorKeyedService sServiceForTesting;
    private static final ProfileKeyedMap<ActorKeyedService> sProfileMap =
            new ProfileKeyedMap<>(
                    ProfileKeyedMap.ProfileSelection.OWN_INSTANCE,
                    ProfileKeyedMap.noRequiredCleanupAction());

    private ActorKeyedServiceFactory() {}

    /**
     * Retrieves the service instance for a given profile. Returns null if the profile is
     * off-the-record or the service is not available.
     */
    @Nullable
    public static ActorKeyedService getForProfile(Profile profile) {
        if (sServiceForTesting != null) return sServiceForTesting;
        return sProfileMap.getForProfile(profile, ActorKeyedServiceFactory::buildForProfile);
    }

    private static ActorKeyedService buildForProfile(Profile profile) {
        // TODO(543155398): This can be null, but fix callers to not call this with unexpected
        // profile or mark this method Nullable.
        return ActorKeyedServiceFactoryJni.get().getForProfile(profile);
    }

    public static void setForTesting(@Nullable ActorKeyedService service) {
        sServiceForTesting = service;
        ResettersForTesting.register(() -> sServiceForTesting = null);
    }

    @NativeMethods
    public interface Natives {
        ActorKeyedService getForProfile(Profile profile);
    }
}
