// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.when;

import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;
import android.os.SystemClock;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.internal.DoNotInstrument;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.base.SplitCompatContentProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.background_task_scheduler.BackgroundTaskScheduler;
import org.chromium.components.background_task_scheduler.BackgroundTaskSchedulerFactory;

import java.io.Closeable;
import java.io.DataInput;
import java.io.DataOutput;
import java.util.concurrent.TimeUnit;

/**
 * Unit tests for the AttributionReportingProviderImpl.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config( // Allows overriding System.currentTimeMillis() via SystemClock.setCurrentTimeMillis()
        instrumentedPackages = "org.chromium.chrome.browser.attribution_reporting",
        manifest = Config.NONE,
        shadows = {AttributionReportingProviderImplRobolectricTest.ShadowProvider.class,
                ShadowSystemClock.class})
@DoNotInstrument
public class AttributionReportingProviderImplRobolectricTest {
    @Implements(SplitCompatContentProvider.Impl.class)
    public static class ShadowProvider {
        @Implementation
        protected String getCallingPackage() {
            return "package";
        }

        @Implementation
        protected Context getContext() {
            return ContextUtils.getApplicationContext();
        }
    }

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    interface Writer extends DataOutput, Closeable {}
    interface Reader extends DataInput, Closeable {}

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    private ImpressionPersistentStore<Writer, Reader> mImpressionStore;

    @Mock
    private BackgroundTaskScheduler mScheduler;

    private static final long INITIAL_TIME =
            TimeUnit.HOURS.toMillis(ImpressionPersistentStore.MIN_REPORTING_INTERVAL_HOURS);

    private static ContentValues makeContentValues(
            String eventId, String destination, String reportTo, Long expiry) {
        ContentValues values = new ContentValues();
        values.put(AttributionConstants.EXTRA_ATTRIBUTION_SOURCE_EVENT_ID, eventId);
        values.put(AttributionConstants.EXTRA_ATTRIBUTION_DESTINATION, destination);
        values.put(AttributionConstants.EXTRA_ATTRIBUTION_REPORT_TO, reportTo);
        values.put(AttributionConstants.EXTRA_ATTRIBUTION_EXPIRY, expiry);
        return values;
    }

    @Before
    public void setUp() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME);
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(mScheduler);
    }

    @After
    public void tearDown() {
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.ATTRIBUTION_PROVIDER_LAST_BROWSER_START);
        BackgroundTaskSchedulerFactory.setSchedulerForTesting(null);
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
    public void testStartFlushTask() throws Exception {
        ContentValues values = makeContentValues("event", "destination", "reportTo", 12345L);
        AttributionReportingProviderImpl impl =
                new AttributionReportingProviderImpl(mImpressionStore);

        when(mImpressionStore.storeImpression(any()))
                .thenReturn(false)
                .thenReturn(true)
                .thenReturn(true)
                .thenReturn(true);

        Assert.assertEquals(Uri.EMPTY, impl.insert(null, values));
        Mockito.verify(mScheduler, times(0)).schedule(any(), any());

        Assert.assertEquals(Uri.EMPTY, impl.insert(null, values));
        Mockito.verify(mScheduler, times(1)).schedule(any(), any());

        Assert.assertEquals(Uri.EMPTY, impl.insert(null, values));
        Mockito.verify(mScheduler, times(1)).schedule(any(), any());

        SystemClock.setCurrentTimeMillis(INITIAL_TIME
                + TimeUnit.HOURS.toMillis(ImpressionPersistentStore.MIN_REPORTING_INTERVAL_HOURS));
        Assert.assertEquals(Uri.EMPTY, impl.insert(null, values));
        Mockito.verify(mScheduler, times(2)).schedule(any(), any());
    }
}
