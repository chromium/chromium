// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.inOrder;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.mockito.stubbing.Answer;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.background_task_scheduler.BackgroundTask;
import org.chromium.content_public.browser.AttributionReporter;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.util.Arrays;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Tests for the AttributionReportingProviderFlushTask.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class AttributionReportingProviderFlushTaskTest {
    private static final String PACKAGE_1 = "org.package1";
    private static final String PACKAGE_2 = "org.package2";
    private static final String EVENT_ID_1 = "12345";
    private static final String EVENT_ID_2 = "23456";
    private static final String DESTINATION_1 = "https://example.com";
    private static final String DESTINATION_2 = "https://other.com";
    private static final String REPORT_TO_1 = "https://report.com";
    private static final String REPORT_TO_2 = null;
    private static final long EXPIRY_1 = 34567;
    private static final long EXPIRY_2 = 0;
    private static final long EVENT_TIME_1 = 5678;
    private static final long EVENT_TIME_2 = 6789;

    private static final AttributionParameters PARAMS_1 = AttributionParameters.forCachedEvent(
            PACKAGE_1, EVENT_ID_1, DESTINATION_1, REPORT_TO_1, EXPIRY_1, EVENT_TIME_1);
    private static final AttributionParameters PARAMS_2 = AttributionParameters.forCachedEvent(
            PACKAGE_2, EVENT_ID_2, DESTINATION_2, REPORT_TO_2, EXPIRY_2, EVENT_TIME_2);

    @Rule
    public AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    public AttributionReporter mAttributionReporter;

    @Mock
    public ImpressionPersistentStore mPersistentStore;

    private AttributionReportingProviderFlushTask mFlushTask;
    private InOrder mInOrder;

    @Before
    public void setUp() {
        AttributionReporter.setInstanceForTesting(mAttributionReporter);
        mFlushTask = new AttributionReportingProviderFlushTask(mPersistentStore);
        mInOrder = inOrder(mAttributionReporter);

        doReturn(Arrays.asList(PARAMS_1, PARAMS_2))
                .when(mPersistentStore)
                .getAndClearStoredImpressions();
    }

    @Test
    @SmallTest
    public void testValidAttribution_Enabled_WithNative() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            BrowserStartupController.getInstance().startBrowserProcessesSync(
                    LibraryProcessType.PROCESS_BROWSER, false);
        });

        AtomicBoolean postedTaskRun = new AtomicBoolean(false);
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> { postedTaskRun.set(true); });
                return null;
            }
        })
                .when(mAttributionReporter)
                .reportAppImpression(any(), eq(PACKAGE_1), eq(EVENT_ID_1), eq(DESTINATION_1),
                        eq(REPORT_TO_1), eq(EXPIRY_1), eq(EVENT_TIME_1));

        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                // Verify that other posted tasks run between attributions being reported when the
                // browser was already started.
                Assert.assertTrue(postedTaskRun.get());
                return null;
            }
        })
                .when(mAttributionReporter)
                .reportAppImpression(any(), eq(PACKAGE_2), eq(EVENT_ID_2), eq(DESTINATION_2),
                        eq(REPORT_TO_2), eq(EXPIRY_2), eq(EVENT_TIME_2));

        CallbackHelper callbackHelper = new CallbackHelper();
        BackgroundTask.TaskFinishedCallback callback = new BackgroundTask.TaskFinishedCallback() {
            @Override
            public void taskFinished(boolean needsReschedule) {
                callbackHelper.notifyCalled();
            }
        };
        startTaskAndVerifyAttributions(callback, callbackHelper);
    }

    @Test
    @SmallTest
    public void testValidAttribution_Enabled_WithoutNative() throws Exception {
        AtomicBoolean postedTaskRun = new AtomicBoolean(false);
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> { postedTaskRun.set(true); });
                return null;
            }
        })
                .when(mAttributionReporter)
                .reportAppImpression(any(), eq(PACKAGE_1), eq(EVENT_ID_1), eq(DESTINATION_1),
                        eq(REPORT_TO_1), eq(EXPIRY_1), eq(EVENT_TIME_1));

        CallbackHelper callbackHelper = new CallbackHelper();
        BackgroundTask.TaskFinishedCallback callback = new BackgroundTask.TaskFinishedCallback() {
            @Override
            public void taskFinished(boolean needsReschedule) {
                // Verify that attributions are synchronously reported when the browser was not
                // already started.
                Assert.assertFalse(postedTaskRun.get());
                callbackHelper.notifyCalled();
            }
        };
        startTaskAndVerifyAttributions(callback, callbackHelper);
    }

    private void startTaskAndVerifyAttributions(BackgroundTask.TaskFinishedCallback callback,
            CallbackHelper callbackHelper) throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> { mFlushTask.onStartTask(null, null, callback); });

        callbackHelper.waitForCallback(0);
        mInOrder.verify(mAttributionReporter)
                .reportAppImpression(any(), eq(PACKAGE_1), eq(EVENT_ID_1), eq(DESTINATION_1),
                        eq(REPORT_TO_1), eq(EXPIRY_1), eq(EVENT_TIME_1));
        mInOrder.verify(mAttributionReporter)
                .reportAppImpression(any(), eq(PACKAGE_2), eq(EVENT_ID_2), eq(DESTINATION_2),
                        eq(REPORT_TO_2), eq(EXPIRY_2), eq(EVENT_TIME_2));
        mInOrder.verifyNoMoreInteractions();
        Mockito.verifyNoMoreInteractions(mAttributionReporter);

        Assert.assertEquals(2,
                RecordHistogram.getHistogramValueCountForTesting(
                        AttributionMetrics.ATTRIBUTION_EVENTS_NAME,
                        AttributionMetrics.AttributionEvent.REPORTED_POST_NATIVE));
    }
}
