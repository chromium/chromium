// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.metrics.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationState;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.TimeUnit;

/** Tests for {@link TrustedWebActivityOpenTimeRecorder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TrustedWebActivityOpenTimeRecorderTest {
    @Mock ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock CurrentPageVerifier mCurrentPageVerifier;
    @Mock TrustedWebActivityUmaRecorder mUmaRecorder;
    @Mock ActivityTabProvider mTabProvider;
    @Captor ArgumentCaptor<Runnable> mVerificationObserverCaptor;

    private TrustedWebActivityOpenTimeRecorder mRecorder;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doNothing()
                .when(mCurrentPageVerifier)
                .addVerificationObserver(mVerificationObserverCaptor.capture());
        mRecorder =
                new TrustedWebActivityOpenTimeRecorder(
                        mLifecycleDispatcher, mCurrentPageVerifier, mUmaRecorder, mTabProvider);

        Tab tab = mock(Tab.class);
        WebContents webContents = mock(WebContents.class);
        when(mTabProvider.get()).thenReturn(tab);
        when(tab.getWebContents()).thenReturn(webContents);
    }

    @Test
    public void recordsTwaOpened() {
        launchTwa();
        verify(mUmaRecorder).recordTwaOpened(any());
    }

    @Test
    public void doesntRecordTwaOpenedTwice() {
        launchTwa();
        leaveVerifiedOrigin();

        clearInvocations(mUmaRecorder);
        returnToVerifiedOrigin();
        verify(mUmaRecorder, never()).recordTwaOpened(any());
    }

    @Test
    public void recordsTwaOpenTime_OnFirstActivityPause() {
        launchTwa();
        advanceTime(3000);

        mRecorder.onPauseWithNative();
        verify(mUmaRecorder).recordTwaOpenTime(3000);
    }

    @Test
    public void recordsTwaOpenTime_OnSecondActivityPause() {
        launchTwa();
        advanceTime(3000);
        mRecorder.onPauseWithNative();
        advanceTime(2000);
        mRecorder.onResumeWithNative();
        advanceTime(4000);

        clearInvocations(mUmaRecorder);
        mRecorder.onPauseWithNative();
        verify(mUmaRecorder).recordTwaOpenTime(4000);
    }

    @Test
    public void recordsTimeInVerified_WhenLeftVerified() {
        launchTwa();
        advanceTime(2000);

        leaveVerifiedOrigin();
        verify(mUmaRecorder).recordTimeInVerifiedOrigin(2000);
    }

    @Test
    public void recordsTimeOutOfVerified_WhenReturnedToVerified() {
        launchTwa();
        advanceTime(2000);
        leaveVerifiedOrigin();
        advanceTime(3000);

        returnToVerifiedOrigin();
        verify(mUmaRecorder).recordTimeOutOfVerifiedOrigin(3000);
    }

    @Test
    public void recordsTimeInVerified_WhenLeftVerifiedAgain() {
        launchTwa();
        advanceTime(2000);
        leaveVerifiedOrigin();
        advanceTime(3000);
        returnToVerifiedOrigin();
        advanceTime(4000);

        clearInvocations(mUmaRecorder);
        leaveVerifiedOrigin();
        verify(mUmaRecorder).recordTimeInVerifiedOrigin(4000);
    }

    @Test
    public void recordsTimeOutOfVerified_WhenReturnedToVerifiedAgain() {
        launchTwa();
        advanceTime(2000);
        leaveVerifiedOrigin();
        advanceTime(3000);
        returnToVerifiedOrigin();
        advanceTime(4000);
        leaveVerifiedOrigin();
        advanceTime(5000);

        clearInvocations(mUmaRecorder);
        returnToVerifiedOrigin();
        verify(mUmaRecorder).recordTimeOutOfVerifiedOrigin(5000);
    }

    @Test
    public void recordsTimeInVerified_WhenPausedWhileInVerified() {
        launchTwa();
        advanceTime(2000);

        mRecorder.onPauseWithNative();
        verify(mUmaRecorder).recordTimeInVerifiedOrigin(2000);
    }

    @Test
    public void recordsTimeInVerified_AfterResumedInVerified_AndLeftVerified() {
        launchTwa();
        advanceTime(2000);
        mRecorder.onPauseWithNative();
        advanceTime(3000);
        mRecorder.onResumeWithNative();
        advanceTime(4000);

        clearInvocations(mUmaRecorder);
        leaveVerifiedOrigin();
        verify(mUmaRecorder).recordTimeInVerifiedOrigin(4000);
    }

    @Test
    public void recordsTimeOutOfVerified_WhenPausedWhileOutOfVerified() {
        launchTwa();
        advanceTime(2000);
        leaveVerifiedOrigin();
        advanceTime(3000);

        mRecorder.onPauseWithNative();
        verify(mUmaRecorder).recordTimeOutOfVerifiedOrigin(3000);
    }

    @Test
    public void doesntRecordAnyTime_WhenVerifiedForFirstTime() {
        launchTwa();
        verify(mUmaRecorder, never()).recordTimeInVerifiedOrigin(anyLong());
        verify(mUmaRecorder, never()).recordTimeOutOfVerifiedOrigin(anyLong());
        verify(mUmaRecorder, never()).recordTwaOpenTime(anyLong());
    }

    private void launchTwa() {
        advanceTime(1000);
        mRecorder.onResumeWithNative();
        setVerificationStatus(VerificationStatus.PENDING);
        setVerificationStatus(VerificationStatus.SUCCESS);
    }

    private void leaveVerifiedOrigin() {
        setVerificationStatus(VerificationStatus.FAILURE);
    }

    private void returnToVerifiedOrigin() {
        setVerificationStatus(VerificationStatus.SUCCESS);
    }

    private void setVerificationStatus(@VerificationStatus int status) {
        VerificationState newState =
                new VerificationState("www.example.com", "www.example.com", status);
        when(mCurrentPageVerifier.getState()).thenReturn(newState);
        for (Runnable observer : mVerificationObserverCaptor.getAllValues()) {
            observer.run();
        }
    }

    private void advanceTime(long millis) {
        Robolectric.getForegroundThreadScheduler().advanceBy(millis, TimeUnit.MILLISECONDS);
    }
}
