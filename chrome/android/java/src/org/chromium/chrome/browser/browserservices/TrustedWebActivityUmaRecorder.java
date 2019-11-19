// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.metrics.UkmRecorder;
import org.chromium.chrome.browser.tab.Tab;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import javax.inject.Inject;

import dagger.Reusable;

/**
 * Encapsulates Uma recording actions related to Trusted Web Activities.
 */
@Reusable
public class TrustedWebActivityUmaRecorder {
    @IntDef({DelegatedNotificationSmallIconFallback.NO_FALLBACK,
            DelegatedNotificationSmallIconFallback.FALLBACK_ICON_NOT_PROVIDED,
            DelegatedNotificationSmallIconFallback.FALLBACK_FOR_STATUS_BAR,
            DelegatedNotificationSmallIconFallback.FALLBACK_FOR_STATUS_BAR_AND_CONTENT})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DelegatedNotificationSmallIconFallback {
        int NO_FALLBACK = 0;
        int FALLBACK_ICON_NOT_PROVIDED = 1;
        int FALLBACK_FOR_STATUS_BAR = 2;
        int FALLBACK_FOR_STATUS_BAR_AND_CONTENT = 3;
        int NUM_ENTRIES = 4;
    }

    @IntDef({ShareRequestMethod.GET, ShareRequestMethod.POST})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ShareRequestMethod {
        int GET = 0;
        int POST = 1;
        int NUM_ENTRIES = 2;
    }

    private final ChromeBrowserInitializer mBrowserInitializer;

    @Inject
    public TrustedWebActivityUmaRecorder(ChromeBrowserInitializer browserInitializer) {
        mBrowserInitializer = browserInitializer;
    }

    /**
     * Records that a Trusted Web Activity has been opened.
     */
    public void recordTwaOpened(@Nullable Tab tab) {
        RecordUserAction.record("BrowserServices.TwaOpened");
        if (tab != null) {
            new UkmRecorder.Bridge().recordEvent(tab.getWebContents(), "TrustedWebActivity.Open");
        }
    }

    /**
     * Records the time that a Trusted Web Activity has been in resumed state.
     */
    public void recordTwaOpenTime(long durationMs) {
        recordDuration(durationMs, "BrowserServices.TwaOpenTime.V2");
    }

    /**
     * Records the time spent in verified origin until navigating to unverified one or pausing
     * the Trusted Web Activity.
     */
    public void recordTimeInVerifiedOrigin(long durationMs) {
        recordDuration(durationMs, "TrustedWebActivity.TimeInVerifiedOrigin.V2");
    }

    /**
     * Records the time spent in verified origin until navigating to unverified one or pausing
     * the Trusted Web Activity.
     */
    public void recordTimeOutOfVerifiedOrigin(long durationMs) {
        recordDuration(durationMs, "TrustedWebActivity.TimeOutOfVerifiedOrigin.V2");
    }

    private void recordDuration(long durationMs, String histogramName) {
        RecordHistogram.recordLongTimesHistogram(histogramName, durationMs);
    }

    /**
     * Records the fact that disclosure was shown.
     */
    public void recordDisclosureShown() {
        RecordUserAction.record("TrustedWebActivity.DisclosureShown");
    }

    /**
     * Records the fact that disclosure was accepted by user.
     */
    public void recordDisclosureAccepted() {
        RecordUserAction.record("TrustedWebActivity.DisclosureAccepted");
    }

    /**
     * Records which action the user took upon seeing a clear data dialog.
     * @param accepted Whether user proceeded to the settings from the dialog.
     * @param triggeredByUninstall Whether the dialog was triggered by app uninstall as opposed to
     * app data getting cleared.
     */
    public void recordClearDataDialogAction(boolean accepted, boolean triggeredByUninstall) {
        String histogramName = triggeredByUninstall
                ? "TrustedWebActivity.ClearDataDialogOnUninstallAccepted"
                : "TrustedWebActivity.ClearDataDialogOnClearAppDataAccepted";
        RecordHistogram.recordBooleanHistogram(histogramName, accepted);
    }

    /**
     * Records the fact that site settings were opened via "Manage Space" button in TWA client app's
     * settings.
     */
    public void recordOpenedSettingsViaManageSpace() {
        doWhenNativeLoaded(() ->
            RecordUserAction.record("TrustedWebActivity.OpenedSettingsViaManageSpace"));
    }

    /**
     * Records which fallback (if any) was used for the small icon of a delegated notification.
     */
    public void recordDelegatedNotificationSmallIconFallback(
            @DelegatedNotificationSmallIconFallback int fallback) {
        RecordHistogram.recordEnumeratedHistogram(
                "TrustedWebActivity.DelegatedNotificationSmallIconFallback", fallback,
                DelegatedNotificationSmallIconFallback.NUM_ENTRIES);
    }

    /**
     * Records whether or not a splash screen has been shown when launching a TWA.
     * Uses {@link TaskTraits#BEST_EFFORT} in order to not get in the way of loading the page.
     */
    public void recordSplashScreenUsage(boolean wasShown) {
        doWhenNativeLoaded(() ->
                PostTask.postTask(TaskTraits.BEST_EFFORT, () ->
                        RecordHistogram.recordBooleanHistogram(
                                "TrustedWebActivity.SplashScreenShown", wasShown)
                ));
    }

    /**
     * Records the fact that data was shared via a TWA.
     */
    public void recordShareTargetRequest(@ShareRequestMethod int method) {
        RecordHistogram.recordEnumeratedHistogram("TrustedWebActivity.ShareTargetRequest",
                method, ShareRequestMethod.NUM_ENTRIES);
    }

    private void doWhenNativeLoaded(Runnable runnable) {
        mBrowserInitializer.runNowOrAfterNativeInitialization(runnable);
    }
}
