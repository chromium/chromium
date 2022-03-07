// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.eq;

import android.content.ContentProviderClient;
import android.content.ContentValues;
import android.content.Context;
import android.net.Uri;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.mockito.stubbing.Answer;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.AttributionReporter;
import org.chromium.content_public.browser.test.util.DOMUtils;

/**
 * Integration tests for the AttributionReportingProvider
 */
@RunWith(ChromeJUnit4ClassRunner.class)
// enable-experimental-web-platform-features turns on the overall ConversionMeasurement Blink
// feature.
// conversions-debug-mode will send reports with no delay or noise.
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
        "enable-experimental-web-platform-features", "conversions-debug-mode"})
@Features.EnableFeatures(ChromeFeatureList.APP_TO_WEB_ATTRIBUTION)
public class AttributionProviderIntegrationTest {
    private static final String EVENT_ID = "12345";
    private static final String CONVERSION_URL = "https://example.test";
    private static final String REPORT_TO_URL = "https://other.test";
    private static final Long EXPIRY = 60000L;

    private static final String BASE_PATH = "/chrome/test/data/android/attribution_reporting/";
    private static final String ATTRIBUTION_PAGE = BASE_PATH + "create_attribution.html";
    private static final String CONVERSION_PAGE = BASE_PATH + "create_conversion.html";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public TestRule mProcessor = new Features.InstrumentationProcessor();

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
        mActivityTestRule.getEmbeddedTestServerRule().setServerUsesHttps(true);
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

    private void testFlushFromNative(String url, boolean needsClick) throws Exception {
        ContentValues values = makeContentValues(EVENT_ID, CONVERSION_URL, REPORT_TO_URL, EXPIRY);
        Uri uri = mContentProviderClient.insert(mContentUri, values);
        Assert.assertEquals(Uri.EMPTY, uri);

        mActivityTestRule.startMainActivityOnBlankPage();

        Mockito.verify(mAttributionReporter, Mockito.times(0))
                .reportAppImpression(any(), any(), any(), any(), any(), anyLong(), anyLong());

        long nativePtr = 1234;
        final CallbackHelper callbackHelper = new CallbackHelper();
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) throws Throwable {
                callbackHelper.notifyCalled();
                return null;
            }
        })
                .when(mAttributionReporter)
                .reportAppImpression(any(),
                        eq(ContextUtils.getApplicationContext().getPackageName()), eq(EVENT_ID),
                        eq(CONVERSION_URL), eq(REPORT_TO_URL), eq(EXPIRY), anyLong());

        mActivityTestRule.loadUrl(url);
        if (needsClick) DOMUtils.clickNode(mActivityTestRule.getWebContents(), "link");
        callbackHelper.waitForCallback(0);
    }

    @Test
    @LargeTest
    public void testFlushFromNative_Attribution() throws Exception {
        testFlushFromNative(mActivityTestRule.getTestServer().getURL(ATTRIBUTION_PAGE), false);
    }

    @Test
    @LargeTest
    public void testFlushFromNative_Conversion() throws Exception {
        testFlushFromNative(mActivityTestRule.getTestServer().getURL(CONVERSION_PAGE), true);
    }
}
