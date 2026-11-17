// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.os.UserHandle;
import android.os.UserManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;

import java.util.List;

/**
 * Utility class to fetch information about system, or system-level information about the bundle.
 */
@JNINamespace("android_webview")
@NullMarked
public class SystemStateUtil {
    /** Returns whether Android has multiple user profiles. */
    @CalledByNative
    public static @MultipleUserProfilesState int getMultipleUserProfilesState() {
        try {
            UserManager userManager =
                    (UserManager)
                            ContextUtils.getApplicationContext()
                                    .getSystemService(Context.USER_SERVICE);
            List<UserHandle> userHandles = userManager.getUserProfiles();
            assert !userHandles.isEmpty();
            return userHandles.size() > 1
                    ? MultipleUserProfilesState.MULTIPLE_PROFILES
                    : MultipleUserProfilesState.SINGLE_PROFILE;
        } catch (SecurityException e) {
            // If we don't have the QUERY_USERS permission, then we can't tell how many profiles
            // there are. See https://crbug.com/332989719 for reference.
            return MultipleUserProfilesState.UNKNOWN;
        }
    }

    @CalledByNative
    @JniType("std::string")
    public static String getProcessName() {
        return ContextUtils.getProcessName();
    }
}
