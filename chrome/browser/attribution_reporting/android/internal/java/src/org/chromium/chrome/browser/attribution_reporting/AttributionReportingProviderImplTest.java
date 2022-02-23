// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.eq;

import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;

import androidx.test.filters.SmallTest;

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

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.content_public.browser.AttributionReporter;
import org.chromium.content_public.browser.BrowserStartupController;

/**
 * Unit tests for the AttributionReportingProviderImpl
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Features.EnableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
public class AttributionReportingProviderImplTest {
    private static final String EVENT_ID = "12345";
    private static final String CONVERSION_URL = "https://example.com";
    private static final String REPORT_TO_URL = "https://other.com";
    private static final Long EXPIRY = 60000L;

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Rule
    public AccountManagerTestRule mAccountManagerTestRule = new AccountManagerTestRule();

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock
    public AttributionReporter mAttributionReporter;

    private ContentProviderClient mContentProviderClient;
    private Uri mContentUri;

    @Before
    public void setUp() {
        Context context = ContextUtils.getApplicationContext();
        String authority = context.getPackageName() + ".AttributionReporting/";
        mContentUri = Uri.parse("content://" + authority);
        mContentProviderClient =
                context.getContentResolver().acquireContentProviderClient(mContentUri);

        AttributionReporter.setInstanceForTesting(mAttributionReporter);
    }

    public static ContentValues makeContentValues(
            String eventId, String destination, String reportTo, Long expiry) {
        ContentValues values = new ContentValues();
        values.put(AttributionConstants.EXTRA_ATTRIBUTION_SOURCE_EVENT_ID, eventId);
        values.put(AttributionConstants.EXTRA_ATTRIBUTION_DESTINATION, destination);
        values.put(AttributionConstants.EXTRA_ATTRIBUTION_REPORT_TO, reportTo);
        values.put(AttributionConstants.EXTRA_ATTRIBUTION_EXPIRY, expiry);
        return values;
    }

    @Test
    @SmallTest
    public void testValidAttribution_Enabled_WithNative() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(() -> {
            BrowserStartupController.getInstance().startBrowserProcessesSync(
                    LibraryProcessType.PROCESS_BROWSER, false);
        });
        ContentValues values = makeContentValues(EVENT_ID, CONVERSION_URL, REPORT_TO_URL, EXPIRY);
        Uri uri = mContentProviderClient.insert(mContentUri, values);
        Assert.assertEquals(Uri.EMPTY, uri);
        Mockito.verify(mAttributionReporter, Mockito.times(1))
                .reportAppImpression(any(),
                        eq(ContextUtils.getApplicationContext().getPackageName()), eq(EVENT_ID),
                        eq(CONVERSION_URL), eq(REPORT_TO_URL), eq(EXPIRY), eq(0L));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        AttributionMetrics.ATTRIBUTION_EVENTS_NAME,
                        AttributionMetrics.AttributionEvent.RECEIVED_WITH_NATIVE));
    }

    @Test
    @SmallTest
    public void testValidAttribution_Enabled_WithoutNative() throws Exception {
        ContentValues values = makeContentValues(EVENT_ID, CONVERSION_URL, REPORT_TO_URL, EXPIRY);
        Uri uri = mContentProviderClient.insert(mContentUri, values);
        Assert.assertEquals(Uri.EMPTY, uri);
        Mockito.verify(mAttributionReporter, Mockito.times(0))
                .reportAppImpression(any(), any(), any(), any(), any(), anyLong(), anyLong());
    }
}
