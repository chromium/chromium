// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.metrics;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.content_settings.ContentSetting;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Encapsulates Uma recording actions related to Trusted Web Activities. */
@NullMarked
public class TrustedWebActivityUmaRecorder {
    @IntDef({ShareRequestMethod.GET, ShareRequestMethod.POST})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ShareRequestMethod {
        int GET = 0;
        int POST = 1;
        int NUM_ENTRIES = 2;
    }

    @IntDef({
        PermissionChanged.NULL_TO_TRUE,
        PermissionChanged.NULL_TO_FALSE,
        PermissionChanged.TRUE_TO_FALSE,
        PermissionChanged.FALSE_TO_TRUE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface PermissionChanged {
        int NULL_TO_FALSE = 0;
        int NULL_TO_TRUE = 1;
        int TRUE_TO_FALSE = 2;
        int FALSE_TO_TRUE = 3;
        int NUM_ENTRIES = 4;
    }

    private TrustedWebActivityUmaRecorder() {}

    /** Records that a Trusted Web Activity has been opened. */
    public static void recordTwaOpened(@Nullable WebContents webContents) {
        RecordUserAction.record("BrowserServices.TwaOpened");
        if (webContents != null) {
            new UkmRecorder(webContents, "TrustedWebActivity.Open")
                    .addBooleanMetric("HasOccurred")
                    .record();
        }
    }

    /** Records the time that a Trusted Web Activity has been in resumed state. */
    public static void recordTwaOpenTime(long durationMs) {
        recordDuration(durationMs, "BrowserServices.TwaOpenTime.V2");
    }

    private static void recordDuration(long durationMs, String histogramName) {
        RecordHistogram.recordLongTimesHistogram(histogramName, durationMs);
    }

    /** Records the fact that disclosure was shown. */
    public static void recordDisclosureShown() {
        RecordUserAction.record("TrustedWebActivity.DisclosureShown");
    }

    /** Records the fact that disclosure was accepted by user. */
    public static void recordDisclosureAccepted() {
        RecordUserAction.record("TrustedWebActivity.DisclosureAccepted");
    }

    /**
     * Records the fact that site settings were opened via "Manage Space" button in TWA client app's
     * settings.
     */
    public static void recordOpenedSettingsViaManageSpace() {
        RecordUserAction.record("TrustedWebActivity.OpenedSettingsViaManageSpace");
    }

    /** Records the notification permission request result for a TWA. */
    public static void recordNotificationPermissionRequestResult(@ContentSetting int settingValue) {
        RecordHistogram.recordEnumeratedHistogram(
                "TrustedWebActivity.Notification.PermissionRequestResult",
                settingValue,
                ContentSetting.NUM_SETTINGS);
    }

    public static void recordExtraCommandSuccess(String command, boolean success) {
        RecordHistogram.recordBooleanHistogram(
                "TrustedWebActivity.ExtraCommandSuccess." + command, success);
    }
}
