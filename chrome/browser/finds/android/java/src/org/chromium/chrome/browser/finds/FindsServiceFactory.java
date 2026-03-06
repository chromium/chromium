// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.finds;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/** Java bridge to the C++ FindsServiceFactory. */
@JNINamespace("finds")
@NullMarked
public class FindsServiceFactory {
    /** Retrieves the FindsService associated with the given Profile. */
    public static @Nullable FindsService getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        return FindsServiceFactoryJni.get().getForProfile(profile);
    }

    @NativeMethods
    public interface Natives {
        @Nullable FindsService getForProfile(@JniType("Profile*") Profile profile);
    }
}
