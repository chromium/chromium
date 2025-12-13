// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility.settings;

import androidx.annotation.IntDef;

import org.jni_zero.NativeMethods;

import org.chromium.base.metrics.RecordHistogram;
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

    public static final String ACCESSIBILITY_CARET_BROWING_HISTOGRAM =
            "Accessibility.Android.CaretBrowsing.SelectedAction";
    public static final String ACCESSIBILITY_CARET_BROWSING_ENABLED_HISTOGRAM =
            "Accessibility.Android.CaretBrowsing.Enabled";

    // AccessibilityCaretBrowsingAction defined in
    // tools/metrics/histograms/metadata/accessibility/enums.xml.
    //
    // LINT.IfChange(AccessibilityCaretBrowsingAction)
    @IntDef({
        AccessibilityCaretBrowsingAction.ENABLED,
        AccessibilityCaretBrowsingAction.DISABLED,
        AccessibilityCaretBrowsingAction.DISMISSED,
        AccessibilityCaretBrowsingAction.COUNT
    })
    public @interface AccessibilityCaretBrowsingAction {
        int ENABLED = 0;
        int DISABLED = 1;
        int DISMISSED = 2;
        int COUNT = 3;
    }

    // LINT.ThenChange(/tools/metrics/histograms/metadata/accessibility/enums.xml:AccessibilityCaretBrowsingAction)

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
        if (enabled) {
            RecordHistogram.recordEnumeratedHistogram(
                    ACCESSIBILITY_CARET_BROWING_HISTOGRAM,
                    AccessibilityCaretBrowsingAction.ENABLED,
                    AccessibilityCaretBrowsingAction.COUNT);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    ACCESSIBILITY_CARET_BROWING_HISTOGRAM,
                    AccessibilityCaretBrowsingAction.DISABLED,
                    AccessibilityCaretBrowsingAction.COUNT);
        }
    }

    /**
     * Sets the preference for showing the caret browsing dialog. This is shown when F7 keyboard
     * shortcut is used.
     *
     * @param profile The user profile for which to set the preference.
     * @param show True to show the dialog in the future, false to hide it.
     */
    public static void setShowCaretBrowsingDialogPreference(
            @Nullable Profile profile, boolean show) {
        if (profile == null) {
            return;
        }
        AccessibilitySettingsBridgeJni.get().setShowCaretBrowsingDialogPreference(profile, show);
    }

    /**
     * @param profile The user profile.
     * @return true if the caret browsing dialog should be shown.
     */
    public static boolean isShowCaretBrowsingDialogPreference(@Nullable Profile profile) {
        if (profile == null) {
            return false;
        }
        return AccessibilitySettingsBridgeJni.get().isShowCaretBrowsingDialogPreference(profile);
    }

    @NativeMethods
    interface Natives {
        boolean isCaretBrowsingEnabled(Profile profile);

        void setCaretBrowsingEnabled(Profile profile, boolean enabled);

        void setShowCaretBrowsingDialogPreference(Profile profile, boolean enabled);

        boolean isShowCaretBrowsingDialogPreference(Profile profile);
    }
}
