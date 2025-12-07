// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Bridges the SupervisedUserService functionality to Java. The service is keyed by the profile, so
 * the bridge offers static methods with the profile argument too. See the SupervisedUserService
 * header file for more details and docs on the individual methods.
 * TODO(crbug.com/427239583): Replace bridge with a service mirror.
 */
@NullMarked
@JNINamespace("supervised_user")
public class SupervisedUserServiceBridge {

    /** Returns true if the user has any of the content filters settings enabled. */
    public static boolean isSupervisedLocally(Profile profile) {
        return SupervisedUserServiceBridgeJni.get().isSupervisedLocally(profile);
    }

    private SupervisedUserServiceBridge() {}

    @NativeMethods
    interface Natives {
        boolean isSupervisedLocally(@JniType("Profile*") Profile profile);
    }
}
