// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.signin.services;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameter;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.annotation.Config;

import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger.Event;
import org.chromium.chrome.browser.signin.services.SigninFlowTimestampsLogger.FlowVariant;

import java.util.Arrays;
import java.util.Collection;

/** Unit tests for {@link SigninFlowTimestampsLogger}. */
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SigninFlowTimestampsLoggerUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    @Parameters
    public static Collection<Object> data() {
        return Arrays.asList(
                new Object[] {FlowVariant.FULLSCREEN, FlowVariant.WEB, FlowVariant.OTHER});
    }

    @Parameter(0)
    public @FlowVariant String mFlowVariant;

    @Test
    public void testRecordTimestamp_noManagementNotice() {
        SigninFlowTimestampsLogger logger = SigninFlowTimestampsLogger.startLogging(mFlowVariant);

        mFakeTimeTestRule.advanceMillis(500);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SignIn.Timestamps." + mFlowVariant + ".SigninCompleted", 500);
        logger.recordTimestamp(Event.SIGNIN_COMPLETED);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordTimestamp_withManagementNotice() {
        SigninFlowTimestampsLogger logger = SigninFlowTimestampsLogger.startLogging(mFlowVariant);

        mFakeTimeTestRule.advanceMillis(200);
        logger.onManagementNoticeShown();

        mFakeTimeTestRule.advanceMillis(300);
        logger.onManagementNoticeAccepted();

        mFakeTimeTestRule.advanceMillis(500);
        // Total duration = 200 + 300 + 500 = 1000ms; Confirmation delay = 300ms
        // Reported duration = 1000 - 300 = 700ms
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SignIn.Timestamps." + mFlowVariant + ".SigninCompleted", 700);

        logger.recordTimestamp(Event.SIGNIN_COMPLETED);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordTimestamp_withManagementNotice_aborted() {
        SigninFlowTimestampsLogger logger = SigninFlowTimestampsLogger.startLogging(mFlowVariant);

        mFakeTimeTestRule.advanceMillis(200);
        logger.onManagementNoticeShown();

        mFakeTimeTestRule.advanceMillis(300);
        logger.onManagementNoticeAccepted();

        mFakeTimeTestRule.advanceMillis(300);
        // Total duration = 200 + 300 + 300 = 800ms; Confirmation delay = 300ms
        // Reported duration = 800 - 300 = 500ms
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SignIn.Timestamps." + mFlowVariant + ".SigninAborted", 500);

        logger.recordTimestamp(Event.SIGNIN_ABORTED);

        histogramWatcher.assertExpected();
    }

    @Test
    public void testRecordTimestamp_managementStatusLoaded() {
        SigninFlowTimestampsLogger logger = SigninFlowTimestampsLogger.startLogging(mFlowVariant);
        mFakeTimeTestRule.advanceMillis(200);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Signin.SignIn.Timestamps." + mFlowVariant + ".ManagementStatusLoaded",
                        200);

        logger.recordTimestamp(Event.MANAGEMENT_STATUS_LOADED);

        histogramWatcher.assertExpected();
    }

    @Test(expected = AssertionError.class)
    public void testOnManagementNoticeAccepted_withoutNoticeShown() {
        SigninFlowTimestampsLogger logger = SigninFlowTimestampsLogger.startLogging(mFlowVariant);

        mFakeTimeTestRule.advanceMillis(500);
        logger.onManagementNoticeAccepted(); // Called without onManagementNoticeShown()

        mFakeTimeTestRule.advanceMillis(500);
        logger.recordTimestamp(Event.SIGNIN_COMPLETED);
    }

    @Test(expected = AssertionError.class)
    public void testOnManagementNoticeShown_withoutNoticeAccepted() {
        SigninFlowTimestampsLogger logger = SigninFlowTimestampsLogger.startLogging(mFlowVariant);

        mFakeTimeTestRule.advanceMillis(500);
        logger.onManagementNoticeShown();

        mFakeTimeTestRule.advanceMillis(500);
        logger.recordTimestamp(
                Event.SIGNIN_COMPLETED); // Called without onManagementNoticeAccepted()
    }
}
