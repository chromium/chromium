// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.os.Build;
import android.os.UserHandle;
import android.os.UserManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;

import java.lang.reflect.Field;
import java.util.List;
import java.util.Set;

/** Utility class to fetch information about system, or system-level information about the bundle. */
@JNINamespace("android_webview")
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
    @SuppressWarnings("DiscouragedPrivateApi")
    public static @PrimaryCpuAbiBitness int getPrimaryCpuAbiBitness() {
        ApplicationInfo applicationInfo = null;
        String packageName = BuildInfo.getInstance().packageName;
        try {
            applicationInfo =
                    ContextUtils.getApplicationContext()
                            .getPackageManager()
                            .getPackageInfo(packageName, 0)
                            .applicationInfo;
            Field primaryCpuAbiField = ApplicationInfo.class.getDeclaredField("primaryCpuAbi");
            String primaryCpuAbi = (String) primaryCpuAbiField.get(applicationInfo);
            if (primaryCpuAbi != null) {
                if (Set.of(Build.SUPPORTED_32_BIT_ABIS).contains(primaryCpuAbi)) {
                    return PrimaryCpuAbiBitness.k32bit;
                } else if (Set.of(Build.SUPPORTED_64_BIT_ABIS).contains(primaryCpuAbi)) {
                    return PrimaryCpuAbiBitness.k64bit;
                }
            }
        } catch (NameNotFoundException | NoSuchFieldException | IllegalAccessException e) {
        }
        return PrimaryCpuAbiBitness.UNKNOWN;
    }
}
