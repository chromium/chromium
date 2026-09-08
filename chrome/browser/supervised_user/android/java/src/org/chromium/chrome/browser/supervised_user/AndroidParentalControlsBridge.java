// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/**
 * Bridges the AndroidParentalControls functionality back to Java. AndroidParentalControls is
 * platform specific specialization for DeviceParentalControls, and tells about the status of
 * parental controls set on the device level, independent of the profile.
 */
@NullMarked
@JNINamespace("supervised_user")
public class AndroidParentalControlsBridge {

    /** Returns true if the user has any of the content filters settings enabled. */
    public static boolean isSupervisedLocally() {
        return AndroidParentalControlsBridgeJni.get().isSupervisedLocally();
    }

    private AndroidParentalControlsBridge() {}

    @NativeMethods
    interface Natives {
        boolean isSupervisedLocally();
    }
}
