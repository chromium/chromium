// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;

/**
 * Bridge to ConsentFlowMetricsHelper (C++) implementation.
 */
@JNINamespace("vr")
public class ConsentFlowMetrics {
    private long mNativeConsentFlowMetricsHelper;

    ConsentFlowMetrics(WebContents contents) {
        mNativeConsentFlowMetricsHelper = ConsentFlowMetricsJni.get().init(contents);
    }

    void logUserAction(@ConsentDialogAction int action) {
        ConsentFlowMetricsJni.get().logUserAction(mNativeConsentFlowMetricsHelper, action);
    }

    void logConsentFlowDurationWhenConsentGranted() {
        ConsentFlowMetricsJni.get().logConsentFlowDurationWhenConsentGranted(
                mNativeConsentFlowMetricsHelper);
    }

    void logConsentFlowDurationWhenConsentNotGranted() {
        ConsentFlowMetricsJni.get().logConsentFlowDurationWhenConsentNotGranted(
                mNativeConsentFlowMetricsHelper);
    }

    void logConsentFlowDurationWhenUserAborted() {
        ConsentFlowMetricsJni.get().logConsentFlowDurationWhenUserAborted(
                mNativeConsentFlowMetricsHelper);
    }

    void onDialogClosedWithConsent(String url, boolean allowed) {
        ConsentFlowMetricsJni.get().onDialogClosedWithConsent(
                mNativeConsentFlowMetricsHelper, url, allowed);
    }

    @NativeMethods
    /* package */ interface Natives {
        long init(WebContents webContents);
        void logUserAction(long nativeConsentFlowMetricsHelper, @ConsentDialogAction int action);
        void logConsentFlowDurationWhenConsentGranted(long nativeConsentFlowMetricsHelper);
        void logConsentFlowDurationWhenConsentNotGranted(long nativeConsentFlowMetricsHelper);
        void logConsentFlowDurationWhenUserAborted(long nativeConsentFlowMetricsHelper);

        void onDialogClosedWithConsent(
                long nativeConsentFlowMetricsHelper, String url, boolean allowed);
    }
}
