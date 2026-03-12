// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.text.TextUtils;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

/** Class that captures all the metrics needed for Send Tab To Self on Android. */
@JNINamespace("send_tab_to_self")
@NullMarked
public class SendTabToSelfMetricsRecorder {
    public static void recordCrossDeviceTabJourney() {
        RecordUserAction.record("MobileCrossDeviceTabJourney");
    }

    public static void recordNotificationShown() {
        SendTabToSelfMetricsRecorderJni.get().recordNotificationShown();
    }

    /**
     * Attaches a scroll observer to the given tab to track scroll volume.
     *
     * @param tab The tab to attach the observer to.
     * @param scrollToTextFragment The scroll-to-text fragment, if any.
     */
    public static void attachScrollObserverToTab(Tab tab, @Nullable String scrollToTextFragment) {
        boolean hasScrollPosition = !TextUtils.isEmpty(scrollToTextFragment);
        recordHasScrollPositionOnOpened(hasScrollPosition);
        if (tab.getWebContents() != null) {
            attachScrollObserver(tab.getWebContents(), hasScrollPosition);
            return;
        }

        // If the web contents are not available yet, attach an observer to wait for the web
        // contents to be available.
        tab.addObserver(
                new EmptyTabObserver() {
                    @Override
                    public void onContentChanged(Tab t) {
                        if (t.getWebContents() != null) {
                            attachScrollObserver(t.getWebContents(), hasScrollPosition);
                            t.removeObserver(this);
                        }
                    }

                    @Override
                    public void onDestroyed(Tab t) {
                        t.removeObserver(this);
                    }
                });
    }

    private static void attachScrollObserver(WebContents webContents, boolean hasScrollPosition) {
        SendTabToSelfMetricsRecorderJni.get().attachScrollObserver(webContents, hasScrollPosition);
    }

    public static void recordHasScrollPositionOnOpened(boolean hasScrollPosition) {
        SendTabToSelfMetricsRecorderJni.get().recordHasScrollPositionOnOpened(hasScrollPosition);
    }

    public static void recordNotificationOpened() {
        RecordUserAction.record("MobileCrossDeviceTabJourney");
        SendTabToSelfMetricsRecorderJni.get().recordNotificationOpened();
    }

    public static void recordNotificationDismissed() {
        SendTabToSelfMetricsRecorderJni.get().recordNotificationDismissed();
    }

    public static void recordNotificationTimedOut() {
        SendTabToSelfMetricsRecorderJni.get().recordNotificationTimedOut();
    }

    public static void recordScrollPositionGenerationOutcome(
            @ScrollPositionGenerationOutcome int outcome) {
        SendTabToSelfMetricsRecorderJni.get().recordScrollPositionGenerationOutcome(outcome);
    }

    public static void recordScrollPositionGenerationTime(long durationMs) {
        SendTabToSelfMetricsRecorderJni.get().recordScrollPositionGenerationTime(durationMs);
    }

    public static void recordScrollPositionSelectorLength(int length) {
        SendTabToSelfMetricsRecorderJni.get().recordScrollPositionSelectorLength(length);
    }

    @NativeMethods
    interface Natives {
        void recordNotificationShown();

        void attachScrollObserver(WebContents webContents, boolean hasScrollPosition);

        void recordHasScrollPositionOnOpened(boolean hasScrollPosition);

        void recordNotificationOpened();

        void recordNotificationDismissed();

        void recordNotificationTimedOut();

        void recordScrollPositionGenerationOutcome(@ScrollPositionGenerationOutcome int outcome);

        void recordScrollPositionGenerationTime(long durationMs);

        void recordScrollPositionSelectorLength(int length);
    }
}
