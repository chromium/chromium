// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.components.metrics.MetricsReportingLevel;

/** Unit tests for {@link UmaSessionStats}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UmaSessionStatsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private UmaSessionStats.Natives mNativeMock;
    @Mock private PrivacyPreferencesManagerImpl mPrivacyManagerMock;

    @Before
    public void setUp() {
        UmaSessionStatsJni.setInstanceForTesting(mNativeMock);
        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyManagerMock);
    }

    @Test
    public void testChangeMetricsReportingState_Boolean() {
        boolean consent = true;
        int calledFrom = ChangeMetricsReportingStateCalledFrom.UI_SETTINGS;

        UmaSessionStats.changeMetricsReportingState(consent, calledFrom);

        verify(mPrivacyManagerMock).setUsageAndCrashReporting(consent);
        verify(mNativeMock).changeMetricsReportingState(eq(consent), eq(calledFrom));
    }

    @Test
    public void testChangeMetricsReportingState_None() {
        int calledFrom = ChangeMetricsReportingStateCalledFrom.UI_FIRST_RUN;

        UmaSessionStats.changeMetricsReportingState(MetricsReportingLevel.NONE, calledFrom);

        verify(mPrivacyManagerMock).setMetricsReportingLevel(MetricsReportingLevel.NONE);
        verify(mNativeMock).changeMetricsReportingState(eq(false), eq(calledFrom));
    }

    @Test
    public void testChangeMetricsReportingState_Basic() {
        int calledFrom = ChangeMetricsReportingStateCalledFrom.UI_FIRST_RUN;

        UmaSessionStats.changeMetricsReportingState(MetricsReportingLevel.BASIC, calledFrom);

        verify(mPrivacyManagerMock).setMetricsReportingLevel(MetricsReportingLevel.BASIC);
        verify(mNativeMock).changeMetricsReportingState(eq(true), eq(calledFrom));
    }

    @Test
    public void testChangeMetricsReportingState_Advanced() {
        int calledFrom = ChangeMetricsReportingStateCalledFrom.UI_FIRST_RUN;

        UmaSessionStats.changeMetricsReportingState(MetricsReportingLevel.ADVANCED, calledFrom);

        verify(mPrivacyManagerMock).setMetricsReportingLevel(MetricsReportingLevel.ADVANCED);
        verify(mNativeMock).changeMetricsReportingState(eq(true), eq(calledFrom));
    }
}
