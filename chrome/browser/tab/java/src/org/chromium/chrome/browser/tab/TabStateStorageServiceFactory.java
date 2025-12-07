// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/** Factory for fetching profile keyed {@link TabStateStorageService}. */
@JNINamespace("tabs")
@NullMarked
public final class TabStateStorageServiceFactory {
    private static @Nullable TabStateStorageService sTabStateStorageServiceForTesting;

    public static TabStateStorageService getForProfile(Profile profile) {
        if (sTabStateStorageServiceForTesting != null) {
            return sTabStateStorageServiceForTesting;
        }

        return TabStateStorageServiceFactoryJni.get().getForProfile(profile);
    }

    private TabStateStorageServiceFactory() {}

    /**
     * @param testService The test service to override with. Pass null to remove override.
     */
    public static void setForTesting(@Nullable TabStateStorageService testService) {
        sTabStateStorageServiceForTesting = testService;
        ResettersForTesting.register(() -> sTabStateStorageServiceForTesting = null);
    }

    @NativeMethods
    interface Natives {
        TabStateStorageService getForProfile(@JniType("Profile*") Profile profile);
    }
}
