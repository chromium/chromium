// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.components.dom_distiller.core.DomDistillerService;

/**
 * DomDistillerServiceFactory maps Profiles to instances of {@link DomDistillerService} instances.
 * Each {@link Profile} will at most have one instance of this service. If the service does not
 * already exist, it will be created on the first access.
 */
@JNINamespace("dom_distiller::android")
public class DomDistillerServiceFactory {
    private static final ProfileKeyedMap<DomDistillerService> sServiceMap =
            new ProfileKeyedMap<>(
                    ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL,
                    ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION);

    /** Returns Java DomDistillerService for given Profile. */
    public static DomDistillerService getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        return sServiceMap.getForProfile(profile, DomDistillerServiceFactory::buildForProfile);
    }

    private static DomDistillerService buildForProfile(Profile profile) {
        return DomDistillerServiceFactoryJni.get().getForProfile(profile);
    }

    @NativeMethods
    interface Natives {
        DomDistillerService getForProfile(@JniType("Profile*") Profile profile);
    }
}
