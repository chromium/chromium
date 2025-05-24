// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity;

import android.os.SystemClock;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.metrics.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationState;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.tab.Tab;

/** Records how long Trusted Web Activities are used for. */
public class TrustedWebActivityOpenTimeRecorder implements PauseResumeWithNativeObserver {
    private final CurrentPageVerifier mCurrentPageVerifier;
    private final ActivityTabProvider mTabProvider;

    private long mOnResumeTimestampMs;

    private boolean mInVerifiedOrigin;
    private boolean mTwaOpenedRecorded;

    public TrustedWebActivityOpenTimeRecorder(
            CurrentPageVerifier currentPageVerifier,
            ActivityTabProvider tabProvider,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mCurrentPageVerifier = currentPageVerifier;
        mTabProvider = tabProvider;
        lifecycleDispatcher.register(this);
        mCurrentPageVerifier.addVerificationObserver(this::onVerificationStateChanged);
    }

    @Override
    public void onResumeWithNative() {
        mOnResumeTimestampMs = SystemClock.elapsedRealtime();
    }

    @Override
    public void onPauseWithNative() {
        assert mOnResumeTimestampMs != 0;
        TrustedWebActivityUmaRecorder.recordTwaOpenTime(
                SystemClock.elapsedRealtime() - mOnResumeTimestampMs);
        mOnResumeTimestampMs = 0;
    }

    private void onVerificationStateChanged() {
        VerificationState state = mCurrentPageVerifier.getState();
        if (state == null || state.status == VerificationStatus.PENDING) {
            return;
        }
        boolean inVerifiedOrigin = state.status == VerificationStatus.SUCCESS;
        if (inVerifiedOrigin == mInVerifiedOrigin) {
            return;
        }
        mInVerifiedOrigin = inVerifiedOrigin;

        if (mInVerifiedOrigin && !mTwaOpenedRecorded) {
            Tab tab = mTabProvider.get();
            if (tab != null) {
                TrustedWebActivityUmaRecorder.recordTwaOpened(tab.getWebContents());
            }
            mTwaOpenedRecorded = true;
        }
    }
}
