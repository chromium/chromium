// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.permissions;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

/** Utility class that interacts with native to retrieve and set permission-related settings. */
public class PermissionSettingsBridge {
    public static boolean shouldShowNotificationsPromo(WebContents webContents) {
        return PermissionSettingsBridgeJni.get()
                .shouldShowNotificationsPromo(
                        Profile.fromWebContents(webContents).getOriginalProfile(), webContents);
    }

    public static void didShowNotificationsPromo(Profile profile) {
        PermissionSettingsBridgeJni.get().didShowNotificationsPromo(profile.getOriginalProfile());
    }

    @NativeMethods
    public interface Natives {
        boolean shouldShowNotificationsPromo(
                @JniType("Profile*") Profile profile, WebContents webContents);

        void didShowNotificationsPromo(@JniType("Profile*") Profile profile);
    }
}
