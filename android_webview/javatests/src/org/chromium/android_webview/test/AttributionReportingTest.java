// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.test;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.net.Uri;
import android.os.Build;
import android.util.Pair;

import androidx.privacysandbox.ads.adservices.java.measurement.MeasurementManagerFutures;
import androidx.privacysandbox.ads.adservices.measurement.SourceRegistrationRequest;
import androidx.privacysandbox.ads.adservices.measurement.WebSourceParams;
import androidx.privacysandbox.ads.adservices.measurement.WebSourceRegistrationRequest;
import androidx.privacysandbox.ads.adservices.measurement.WebTriggerParams;
import androidx.privacysandbox.ads.adservices.measurement.WebTriggerRegistrationRequest;
import androidx.test.filters.LargeTest;
import androidx.test.filters.SmallTest;

import com.google.common.util.concurrent.Futures;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.settings.AttributionBehavior;
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content.browser.AttributionOsLevelManager;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer.OnPageFinishedHelper;
import org.chromium.content_public.common.ContentFeatures;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.services.network.NetworkServiceFeatures;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

@RunWith(AwJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class AttributionReportingTest {
    private static final String SOURCE_REGISTRATION_PATH = "/source";
    private static final String TRIGGER_REGISTRATION_PATH = "/trigger";
    private static final String OS_SOURCE_RESPONSE_HEADER =
            "Attribution-Reporting-Register-OS-Source";
    private static final String OS_TRIGGER_RESPONSE_HEADER =
            "Attribution-Reporting-Register-OS-Trigger";
    private static final String SOURCE_REGISTRATION_URL = "https://adtech.example/register/source";
    private static final String TRIGGER_REGISTRATION_URL =
            "https://adtech.example/register/trigger";

    @Rule public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    @Mock private MeasurementManagerFutures mMockAttributionManager;

    private CallbackHelper mMockCallbackHelper;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private AwSettings mSettings;

    private TestWebServer mWebServer;
    private TestWebServer mAttributionServer;
    private String mTestPage;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mMockCallbackHelper = new CallbackHelper();

        when(mMockAttributionManager.registerWebSourceAsync(
                        any(WebSourceRegistrationRequest.class)))
                .thenAnswer(
                        invocation -> {
                            mMockCallbackHelper.notifyCalled();
                            return Futures.immediateFuture(null);
                        });
        when(mMockAttributionManager.registerSourceAsync(any(SourceRegistrationRequest.class)))
                .thenAnswer(
                        invocation -> {
                            mMockCallbackHelper.notifyCalled();
                            return Futures.immediateFuture(null);
                        });
        when(mMockAttributionManager.registerWebTriggerAsync(
                        any(WebTriggerRegistrationRequest.class)))
                .thenAnswer(
                        invocation -> {
                            mMockCallbackHelper.notifyCalled();
                            return Futures.immediateFuture(null);
                        });
        when(mMockAttributionManager.registerTriggerAsync(any(Uri.class)))
                .thenAnswer(
                        invocation -> {
                            mMockCallbackHelper.notifyCalled();
                            return Futures.immediateFuture(null);
                        });

        AttributionOsLevelManager.setManagerForTesting(mMockAttributionManager);

        mContentsClient = new TestAwContentsClient();
        AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(
                        mContentsClient, false, new TestDependencyFactory());
        mAwContents = testContainerView.getAwContents();
        mSettings = mActivityTestRule.getAwSettingsOnUiThread(mAwContents);

        mWebServer = TestWebServer.start();
        mAttributionServer = TestWebServer.startAdditional();
        mTestPage = mWebServer.setResponse("/test", createTestPage(), null);
    }

    @After
    public void tearDown() {
        mWebServer.shutdown();
        mAttributionServer.shutdown();
    }

    @SmallTest
    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    @CommandLineFlags.Add(
            "enable-features="
                    + ContentFeatures.PRIVACY_SANDBOX_ADS_AP_IS_OVERRIDE
                    + ","
                    + NetworkServiceFeatures.ATTRIBUTION_REPORTING_CROSS_APP_WEB)
    public void testDefaultBehavior() throws Exception {
        assertEquals(
                AttributionBehavior.APP_SOURCE_AND_WEB_TRIGGER, mSettings.getAttributionBehavior());
    }

    @LargeTest
    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    @CommandLineFlags.Add(
            "enable-features="
                    + ContentFeatures.PRIVACY_SANDBOX_ADS_AP_IS_OVERRIDE
                    + ","
                    + NetworkServiceFeatures.ATTRIBUTION_REPORTING_CROSS_APP_WEB)
    public void testDisabledBehavior() throws Exception {
        mSettings.setAttributionBehavior(AttributionBehavior.DISABLED);
        assertEquals(AttributionBehavior.DISABLED, mSettings.getAttributionBehavior());

        loadUrlSync(mTestPage);

        // When disabled, we don't expect any calls to the attribution server.
        Assert.assertEquals(0, mAttributionServer.getRequestCount(SOURCE_REGISTRATION_PATH));
        Assert.assertEquals(0, mAttributionServer.getRequestCount(TRIGGER_REGISTRATION_PATH));

        // When disabled, we don't expect any calls to any of the actual registration methods.
        verify(mMockAttributionManager, never())
                .registerWebSourceAsync(
                        new WebSourceRegistrationRequest(
                                Arrays.asList(
                                        new WebSourceParams(
                                                Uri.parse(SOURCE_REGISTRATION_URL), false)),
                                Uri.parse(mWebServer.getBaseUrl()),
                                null,
                                null,
                                null,
                                null));
        verify(mMockAttributionManager, never())
                .registerSourceAsync(any(SourceRegistrationRequest.class));
        verify(mMockAttributionManager, never())
                .registerWebTriggerAsync(
                        eq(
                                new WebTriggerRegistrationRequest(
                                        Arrays.asList(
                                                new WebTriggerParams(
                                                        Uri.parse(TRIGGER_REGISTRATION_URL),
                                                        false)),
                                        Uri.parse(mWebServer.getBaseUrl()))));
        verify(mMockAttributionManager, never())
                .registerTriggerAsync(Uri.parse(TRIGGER_REGISTRATION_URL));
    }

    @LargeTest
    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    @CommandLineFlags.Add(
            "enable-features="
                    + ContentFeatures.PRIVACY_SANDBOX_ADS_AP_IS_OVERRIDE
                    + ","
                    + NetworkServiceFeatures.ATTRIBUTION_REPORTING_CROSS_APP_WEB)
    public void testAppSourceAndWebTriggerBehavior() throws Exception {
        mSettings.setAttributionBehavior(AttributionBehavior.APP_SOURCE_AND_WEB_TRIGGER);
        assertEquals(
                AttributionBehavior.APP_SOURCE_AND_WEB_TRIGGER, mSettings.getAttributionBehavior());

        int callBackCount = mMockCallbackHelper.getCallCount();
        loadUrlSync(mTestPage);
        // waiting for one source and one trigger event
        mMockCallbackHelper.waitForCallback(callBackCount, 2);

        verify(mMockAttributionManager, never())
                .registerWebSourceAsync(
                        new WebSourceRegistrationRequest(
                                Arrays.asList(
                                        new WebSourceParams(
                                                Uri.parse(SOURCE_REGISTRATION_URL), false)),
                                Uri.parse(mWebServer.getBaseUrl()),
                                null,
                                null,
                                null,
                                null));
        SourceRegistrationRequest expectedRequest =
                new SourceRegistrationRequest(
                        Arrays.asList(Uri.parse(SOURCE_REGISTRATION_URL)), null);
        verify(mMockAttributionManager, times(1)).registerSourceAsync(eq(expectedRequest));
        verify(mMockAttributionManager, times(1))
                .registerWebTriggerAsync(
                        eq(
                                new WebTriggerRegistrationRequest(
                                        Arrays.asList(
                                                new WebTriggerParams(
                                                        Uri.parse(TRIGGER_REGISTRATION_URL),
                                                        false)),
                                        Uri.parse(mWebServer.getBaseUrl()))));
        verify(mMockAttributionManager, never())
                .registerTriggerAsync(Uri.parse(TRIGGER_REGISTRATION_URL));
    }

    @LargeTest
    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    @CommandLineFlags.Add(
            "enable-features="
                    + ContentFeatures.PRIVACY_SANDBOX_ADS_AP_IS_OVERRIDE
                    + ","
                    + NetworkServiceFeatures.ATTRIBUTION_REPORTING_CROSS_APP_WEB)
    public void testWebSourceAndWebTriggerBehavior() throws Exception {
        mSettings.setAttributionBehavior(AttributionBehavior.WEB_SOURCE_AND_WEB_TRIGGER);
        assertEquals(
                AttributionBehavior.WEB_SOURCE_AND_WEB_TRIGGER, mSettings.getAttributionBehavior());

        int callBackCount = mMockCallbackHelper.getCallCount();
        loadUrlSync(mTestPage);
        // waiting for one source and one trigger event
        mMockCallbackHelper.waitForCallback(callBackCount, 2);

        verify(mMockAttributionManager, times(1))
                .registerWebSourceAsync(
                        new WebSourceRegistrationRequest(
                                Arrays.asList(
                                        new WebSourceParams(
                                                Uri.parse(SOURCE_REGISTRATION_URL), false)),
                                Uri.parse(mWebServer.getBaseUrl()),
                                null,
                                null,
                                null,
                                null));
        verify(mMockAttributionManager, never())
                .registerSourceAsync(any(SourceRegistrationRequest.class));
        verify(mMockAttributionManager, times(1))
                .registerWebTriggerAsync(
                        eq(
                                new WebTriggerRegistrationRequest(
                                        Arrays.asList(
                                                new WebTriggerParams(
                                                        Uri.parse(TRIGGER_REGISTRATION_URL),
                                                        false)),
                                        Uri.parse(mWebServer.getBaseUrl()))));
        verify(mMockAttributionManager, never())
                .registerTriggerAsync(Uri.parse(TRIGGER_REGISTRATION_URL));
    }

    @LargeTest
    @Test
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    @CommandLineFlags.Add(
            "enable-features="
                    + ContentFeatures.PRIVACY_SANDBOX_ADS_AP_IS_OVERRIDE
                    + ","
                    + NetworkServiceFeatures.ATTRIBUTION_REPORTING_CROSS_APP_WEB)
    public void testAppSourceAndAppTriggerBehavior() throws Exception {
        mSettings.setAttributionBehavior(AttributionBehavior.APP_SOURCE_AND_APP_TRIGGER);
        assertEquals(
                AttributionBehavior.APP_SOURCE_AND_APP_TRIGGER, mSettings.getAttributionBehavior());

        int callBackCount = mMockCallbackHelper.getCallCount();
        loadUrlSync(mTestPage);
        // waiting for one source and one trigger event
        mMockCallbackHelper.waitForCallback(callBackCount, 2);

        verify(mMockAttributionManager, never())
                .registerWebSourceAsync(
                        new WebSourceRegistrationRequest(
                                Arrays.asList(
                                        new WebSourceParams(
                                                Uri.parse(SOURCE_REGISTRATION_URL), false)),
                                Uri.parse(mWebServer.getBaseUrl()),
                                null,
                                null,
                                null,
                                null));

        SourceRegistrationRequest expectedRequest =
                new SourceRegistrationRequest(
                        Arrays.asList(Uri.parse(SOURCE_REGISTRATION_URL)), null);
        verify(mMockAttributionManager, times(1)).registerSourceAsync(eq(expectedRequest));
        verify(mMockAttributionManager, never())
                .registerWebTriggerAsync(
                        eq(
                                new WebTriggerRegistrationRequest(
                                        Arrays.asList(
                                                new WebTriggerParams(
                                                        Uri.parse(TRIGGER_REGISTRATION_URL),
                                                        false)),
                                        Uri.parse(mWebServer.getBaseUrl()))));
        verify(mMockAttributionManager, times(1))
                .registerTriggerAsync(Uri.parse(TRIGGER_REGISTRATION_URL));
    }

    private List<Pair<String, String>> getAttributionResponseHeaders(String header, String value) {
        List<Pair<String, String>> headers = new ArrayList<Pair<String, String>>();
        headers.add(Pair.create(header, "\"" + value + "\""));
        return headers;
    }

    private String createTestPage() {
        String sourceUrl =
                mAttributionServer.setResponse(
                        SOURCE_REGISTRATION_PATH,
                        "",
                        getAttributionResponseHeaders(
                                OS_SOURCE_RESPONSE_HEADER, SOURCE_REGISTRATION_URL));
        String triggerUrl =
                mAttributionServer.setResponse(
                        TRIGGER_REGISTRATION_PATH,
                        "",
                        getAttributionResponseHeaders(
                                OS_TRIGGER_RESPONSE_HEADER, TRIGGER_REGISTRATION_URL));

        StringBuilder sb = new StringBuilder();
        sb.append("<html><head></head><body>Hello world!");
        sb.append("<img attributionsrc='").append(sourceUrl).append("'>");
        sb.append("<img attributionsrc='").append(triggerUrl).append("'>");
        sb.append("</body></html>");
        return sb.toString();
    }

    private void loadUrlSync(String requestUrl) throws Exception {
        OnPageFinishedHelper onPageFinishedHelper = mContentsClient.getOnPageFinishedHelper();
        mActivityTestRule.loadUrlSync(mAwContents, onPageFinishedHelper, requestUrl);
        Assert.assertEquals(requestUrl, onPageFinishedHelper.getUrl());
    }
}
