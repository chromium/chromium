// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.mockito.AdditionalAnswers.answerVoid;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.browserservices.metrics.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/** Tests for {@link ClearDataDialogResultRecorder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ClearDataDialogResultRecorderTest {
    private final SharedPreferencesManager mPrefsManager = ChromeSharedPreferences.getInstance();
    @Mock ChromeBrowserInitializer mBrowserInitializer;
    @Mock TrustedWebActivityUmaRecorder mUmaRecorder;
    @Captor ArgumentCaptor<Runnable> mTaskOnNativeInitCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ChromeBrowserInitializer.setForTesting(mBrowserInitializer);
        when(mBrowserInitializer.isFullBrowserInitialized()).thenReturn(false);
        doNothing()
                .when(mBrowserInitializer)
                .runNowOrAfterFullBrowserStarted(mTaskOnNativeInitCaptor.capture());
    }

    @Test
    public void records_WhenAccepted_AfterNativeInit() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "TrustedWebActivity.ClearDataDialogOnUninstallAccepted", true)
                        .build();
        ClearDataDialogResultRecorder.handleDialogResult(true, true);
        finishNativeInit();
        histogramWatcher.assertExpected();
    }

    @Test
    public void records_WhenAccepted_IfNativeAlreadyInited() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "TrustedWebActivity.ClearDataDialogOnUninstallAccepted", true)
                        .build();
        finishNativeInit();
        ClearDataDialogResultRecorder.handleDialogResult(true, true);
        histogramWatcher.assertExpected();
    }

    @Test
    public void records_WhenDismissed_IfNativeAlreadyInited() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "TrustedWebActivity.ClearDataDialogOnUninstallAccepted", false)
                        .build();
        finishNativeInit();
        ClearDataDialogResultRecorder.handleDialogResult(false, true);
        histogramWatcher.assertExpected();
    }

    @Test
    public void defersRecording_WhenDismissed_IfNativeNotAlreadyInited() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("TrustedWebActivity.ClearDataDialogOnUninstallAccepted")
                        .expectNoRecords("TrustedWebActivity.ClearDataDialogOnClearAppDataAccepted")
                        .build();
        ClearDataDialogResultRecorder.handleDialogResult(false, true);
        histogramWatcher.assertExpected();
    }

    @Test
    public void makesDeferredRecordingOfDismissals() {
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecordTimes(
                                "TrustedWebActivity.ClearDataDialogOnUninstallAccepted", false, 2)
                        .expectBooleanRecord(
                                "TrustedWebActivity.ClearDataDialogOnClearAppDataAccepted", false)
                        .build();
        ClearDataDialogResultRecorder.handleDialogResult(false, true);
        ClearDataDialogResultRecorder.handleDialogResult(false, true);
        ClearDataDialogResultRecorder.handleDialogResult(false, false);

        ClearDataDialogResultRecorder.makeDeferredRecordings();
        histogramWatcher.assertExpected();
    }

    @Test
    public void doesntMakeDeferredRecordingTwice() {
        ClearDataDialogResultRecorder.handleDialogResult(false, true);
        ClearDataDialogResultRecorder.makeDeferredRecordings();

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("TrustedWebActivity.ClearDataDialogOnUninstallAccepted")
                        .expectNoRecords("TrustedWebActivity.ClearDataDialogOnClearAppDataAccepted")
                        .build();
        ClearDataDialogResultRecorder.makeDeferredRecordings();
        histogramWatcher.assertExpected();
    }

    private void finishNativeInit() {
        for (Runnable task : mTaskOnNativeInitCaptor.getAllValues()) {
            task.run();
        }
        when(mBrowserInitializer.isFullBrowserInitialized()).thenReturn(true);
        doAnswer(answerVoid(Runnable::run))
                .when(mBrowserInitializer)
                .runNowOrAfterFullBrowserStarted(any());
    }
}
