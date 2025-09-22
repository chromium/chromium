// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility.settings;

import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * JNI bridge for accessibility settings. For the caret browsing feature, it allows users to
 * navigate web content using a movable cursor (caret). This class provides a Java interface to the
 * native caret browsing implementation, which is necessary because the Accessibility Settings are
 * on Java side, while the feature is on the native side.
 */
@NullMarked
public class AccessibilitySettingsBridge {
    /**
     * Checks if the caret browsing feature is currently enabled for a given profile.
     *
     * @param profile The user profile to check.
     * @return True if caret browsing is enabled, false otherwise.
     */
    public static boolean isCaretBrowsingEnabled(@Nullable Profile profile) {
        if (profile == null) {
            return false;
        }
        return AccessibilitySettingsBridgeJni.get().isCaretBrowsingEnabled(profile);
    }

    /**
     * Sets the enabled state of the caret browsing feature for a given profile.
     *
     * @param profile The user profile for which to set the feature.
     * @param enabled True to enable caret browsing, false to disable it.
     */
    public static void setCaretBrowsingEnabled(@Nullable Profile profile, boolean enabled) {
        if (profile == null) {
            return;
        }
        AccessibilitySettingsBridgeJni.get().setCaretBrowsingEnabled(profile, enabled);
    }

    @NativeMethods
    interface Natives {
        boolean isCaretBrowsingEnabled(Profile profile);

        void setCaretBrowsingEnabled(Profile profile, boolean enabled);
    }
}
