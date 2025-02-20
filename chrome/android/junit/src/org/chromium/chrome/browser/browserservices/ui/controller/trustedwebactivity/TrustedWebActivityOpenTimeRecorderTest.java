// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity;

import static org.mockito.Mockito.argThat;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationState;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.components.ukm.UkmRecorderJni;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.TimeUnit;

/** Tests for {@link TrustedWebActivityOpenTimeRecorder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TrustedWebActivityOpenTimeRecorderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock ActivityLifecycleDispatcher mLifecycleDispatcher;
    @Mock CurrentPageVerifier mCurrentPageVerifier;
    @Mock ActivityTabProvider mTabProvider;
    @Captor ArgumentCaptor<Runnable> mVerificationObserverCaptor;
    @Mock UkmRecorder.Natives mUkmRecorderJniMock;
    @Mock WebContents mWebContents;
    @Mock Tab mTab;

    private TrustedWebActivityOpenTimeRecorder mRecorder;

    @Before
    public void setUp() {
        UkmRecorderJni.setInstanceForTesting(mUkmRecorderJniMock);

        doNothing()
                .when(mCurrentPageVerifier)
                .addVerificationObserver(mVerificationObserverCaptor.capture());
        mRecorder =
                new TrustedWebActivityOpenTimeRecorder(
                        mCurrentPageVerifier, mTabProvider, mLifecycleDispatcher);

        when(mTabProvider.get()).thenReturn(mTab);
        when(mTab.getWebContents()).thenReturn(mWebContents);
    }

    private void verifyUkm(String event) {
        verify(mUkmRecorderJniMock)
                .recordEventWithMultipleMetrics(
                        eq(mWebContents),
                        eq(event),
                        argThat(
                                metricsList ->
                                        metricsList.length == 1
                                                && metricsList[0].mName.equals("HasOccurred")
                                                && metricsList[0].mValue == 1));
    }

    @Test
    public void recordsTwaOpened() {
        launchTwa();
        verifyUkm("TrustedWebActivity.Open");
    }

    @Test
    public void doesntRecordTwaOpenedTwice() {
        launchTwa();
        leaveVerifiedOrigin();

        clearInvocations(mUkmRecorderJniMock);
        returnToVerifiedOrigin();
        verifyNoMoreInteractions(mUkmRecorderJniMock);
    }

    @Test
    public void recordsTwaOpenTime_OnFirstActivityPause() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("BrowserServices.TwaOpenTime.V2", 3000)
                        .build();
        launchTwa();
        advanceTime(3000);

        mRecorder.onPauseWithNative();
        histogramWatcher.assertExpected();
    }

    @Test
    public void recordsTwaOpenTime_OnSecondActivityPause() {
        launchTwa();
        advanceTime(3000);
        mRecorder.onPauseWithNative();
        advanceTime(2000);
        mRecorder.onResumeWithNative();
        advanceTime(4000);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("BrowserServices.TwaOpenTime.V2", 4000)
                        .build();
        mRecorder.onPauseWithNative();
        histogramWatcher.assertExpected();
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
