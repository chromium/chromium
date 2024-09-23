// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;

/** Utility class for defining features and params related to tab group sync. */
@JNINamespace("tab_groups")
public final class TabGroupSyncFeatures {
    /** Whether tab group sync is enabled. */
    public static boolean isTabGroupSyncEnabled(Profile profile) {
        if (profile.isOffTheRecord()) return false;
        return TabGroupSyncFeaturesJni.get().isTabGroupSyncEnabled(profile);
    }

    @NativeMethods
    public interface Natives {
        boolean isTabGroupSyncEnabled(@JniType("Profile*") Profile profile);
    }
}
