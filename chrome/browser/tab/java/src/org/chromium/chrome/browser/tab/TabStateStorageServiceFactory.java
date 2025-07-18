// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/** Factory for fetching profile keyed {@link TabStateStorageService}. */
@JNINamespace("tabs")
@NullMarked
public final class TabStateStorageServiceFactory {
    public static TabStateStorageService getForProfile(Profile profile) {
        return TabStateStorageServiceFactoryJni.get().getForProfile(profile);
    }

    private TabStateStorageServiceFactory() {}

    @NativeMethods
    interface Natives {
        TabStateStorageService getForProfile(@JniType("Profile*") Profile profile);
    }
}
