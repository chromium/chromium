// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.metrics.RecordUserAction;

/**
 * Class that captures all the metrics needed for Send Tab To Self on Android.
 */
@JNINamespace("send_tab_to_self")
class MetricsRecorder {
    public static void recordSendingEvent(@SendingEvent int sendingEvent) {
        RecordUserAction.record("MobileCrossDeviceTabOpenedOrSent");
        MetricsRecorderJni.get().recordSendingEvent(sendingEvent);
    }

    public static void recordNotificationShown() {
        MetricsRecorderJni.get().recordNotificationShown();
    }

    public static void recordNotificationOpened() {
        RecordUserAction.record("MobileCrossDeviceTabOpenedOrSent");
        MetricsRecorderJni.get().recordNotificationOpened();
    }

    public static void recordNotificationDismissed() {
        MetricsRecorderJni.get().recordNotificationDismissed();
    }

    public static void recordNotificationTimedOut() {
        MetricsRecorderJni.get().recordNotificationTimedOut();
    }

    @NativeMethods
    interface Natives {
        void recordSendingEvent(int sendingEvent);
        void recordNotificationShown();
        void recordNotificationOpened();
        void recordNotificationDismissed();
        void recordNotificationTimedOut();
    }
}
