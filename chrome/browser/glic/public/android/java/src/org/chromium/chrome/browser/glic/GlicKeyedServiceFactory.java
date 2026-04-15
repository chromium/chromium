// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.glic;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/** This factory creates GlicKeyedService for the given {@link Profile}. */
@JNINamespace("glic")
@NullMarked
public class GlicKeyedServiceFactory {
    @Nullable private static GlicKeyedService sServiceForTesting;

    private GlicKeyedServiceFactory() {}

    /** Returns The GlicKeyedService for the given profile. */
    public static @Nullable GlicKeyedService getForProfile(Profile profile) {
        if (sServiceForTesting != null) return sServiceForTesting;
        return GlicKeyedServiceFactoryJni.get().getForProfile(profile);
    }

    /** Sets the service instance for testing. */
    public static void setForTesting(@Nullable GlicKeyedService service) {
        sServiceForTesting = service;
        ResettersForTesting.register(() -> sServiceForTesting = null);
    }

    @NativeMethods
    interface Natives {
        GlicKeyedService getForProfile(@JniType("Profile*") Profile profile);
    }
}
