// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.os.UserHandle;
import android.os.UserManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.util.List;

/** Utility class to fetch information about system. */
@JNINamespace("android_webview")
public class SystemStateUtil {
    /** Returns whether Android has multiple user profiles. */
    @CalledByNative
    public static @MultipleUserProfilesState int getMultipleUserProfilesState() {
        UserManager userManager =
                (UserManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.USER_SERVICE);
        List<UserHandle> userHandles = userManager.getUserProfiles();
        assert !userHandles.isEmpty();
        return userHandles.size() > 1 ? MultipleUserProfilesState.MULTIPLE_PROFILES
                                      : MultipleUserProfilesState.SINGLE_PROFILE;
    }
}
