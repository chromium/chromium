// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.long_screenshots.bitmap_generation;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * The Java-side implementations of long_screenshots_tab_service_factory.cc. Provides an instance of
 * {@link LongScreenshotsTabService}.
 */
@JNINamespace("long_screenshots")
@NullMarked
public class LongScreenshotsTabServiceFactory {
    public static LongScreenshotsTabService getServiceInstance(Profile profile) {
        return LongScreenshotsTabServiceFactoryJni.get().getServiceInstanceForProfile(profile);
    }

    @NativeMethods
    interface Natives {
        LongScreenshotsTabService getServiceInstanceForProfile(
                @JniType("Profile*") Profile profile);
    }

    private LongScreenshotsTabServiceFactory() {}
}
