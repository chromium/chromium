// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

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
import org.chromium.chrome.browser.metrics.ChangeMetricsReportingStateCalledFrom;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.metrics.UmaSessionStatsJni;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.components.metrics.MetricsReportingLevel;

/** Unit tests for {@link FirstRunUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FirstRunUtilsUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private FirstRunUtils.Natives mFirstRunUtilsNativeMock;
    @Mock private UmaSessionStats.Natives mUmaSessionStatsNativeMock;
    @Mock private PrivacyPreferencesManagerImpl mPrivacyManagerMock;

    @Before
    public void setUp() {
        FirstRunUtilsJni.setInstanceForTesting(mFirstRunUtilsNativeMock);
        UmaSessionStatsJni.setInstanceForTesting(mUmaSessionStatsNativeMock);
        PrivacyPreferencesManagerImpl.setInstanceForTesting(mPrivacyManagerMock);
    }

    @Test
    public void testAcceptTermsOfService_AllowMetricsTrue() {
        FirstRunUtils.acceptTermsOfService(true);

        verify(mPrivacyManagerMock).setMetricsReportingLevel(MetricsReportingLevel.BASIC);
        verify(mUmaSessionStatsNativeMock)
                .changeMetricsReportingState(
                        eq(true), eq(ChangeMetricsReportingStateCalledFrom.UI_FIRST_RUN));
        verify(mFirstRunUtilsNativeMock).setEulaAccepted();
    }

    @Test
    public void testAcceptTermsOfService_AllowMetricsFalse() {
        FirstRunUtils.acceptTermsOfService(false);

        verify(mPrivacyManagerMock).setMetricsReportingLevel(MetricsReportingLevel.NONE);
        verify(mUmaSessionStatsNativeMock)
                .changeMetricsReportingState(
                        eq(false), eq(ChangeMetricsReportingStateCalledFrom.UI_FIRST_RUN));
        verify(mFirstRunUtilsNativeMock).setEulaAccepted();
    }
}
