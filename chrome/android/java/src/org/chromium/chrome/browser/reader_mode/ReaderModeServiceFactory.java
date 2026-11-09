// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.reader_mode;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.components.web_distiller.core.WebDistillerService;

/**
 * ReaderModeServiceFactory maps Profiles to instances of {@link WebDistillerService} instances.
 * Each {@link Profile} will at most have one instance of this service. If the service does not
 * already exist, it will be created on the first access.
 */
@JNINamespace("reader_mode::android")
@NullMarked
public class ReaderModeServiceFactory {
    private static final ProfileKeyedMap<WebDistillerService> sServiceMap =
            new ProfileKeyedMap<>(
                    ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL,
                    ProfileKeyedMap.noRequiredCleanupAction());

    /** Returns Java WebDistillerService for given Profile. */
    public static WebDistillerService getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        return sServiceMap.getForProfile(profile, ReaderModeServiceFactory::buildForProfile);
    }

    private static WebDistillerService buildForProfile(Profile profile) {
        return ReaderModeServiceFactoryJni.get().getForProfile(profile);
    }

    @NativeMethods
    interface Natives {
        WebDistillerService getForProfile(@JniType("Profile*") Profile profile);
    }
}
