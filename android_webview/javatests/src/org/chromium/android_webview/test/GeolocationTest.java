// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.Context;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwGeolocationPermissions;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.device.geolocation.LocationProviderOverrider;
import org.chromium.device.geolocation.MockLocationProvider;

import java.util.concurrent.Callable;

/**
 * Test suite for Geolocation in AwContents. Smoke tests for
 * basic functionality, and tests to ensure the AwContents.onPause
 * and onResume APIs affect Geolocation as expected.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class GeolocationTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    public GeolocationTest(AwSettingsMutation param) {
        mActivityTestRule =
                new AwActivityTestRule(param.getMutation()) {
                    @Override
                    public TestDependencyFactory createTestDependencyFactory() {
                        return mOverriddenFactory == null
                                ? new TestDependencyFactory()
                                : mOverriddenFactory;
                    }
                };
    }

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;
    private MockLocationProvider mMockLocationProvider;
    private TestDependencyFactory mOverriddenFactory;

    private static final String RAW_HTML =
            "<!DOCTYPE html>\n"
                    + "<html>\n"
                    + "  <head>\n"
                    + "    <title>Geolocation</title>\n"
                    + "    <script>\n"
                    + "      var positionCount = 0;\n"
                    + "      function gotPos(position) {\n"
                    + "        positionCount++;\n"
                    + "      }\n"
                    + "      function errorCallback(error){"
                    + "        window.document.title = 'deny';"
                    + "        console.log('navigator.getCurrentPosition error: ', error);"
                    + "      }"
                    + "      function initiate_getCurrentPosition() {\n"
                    + "        navigator.geolocation.getCurrentPosition(\n"
                    + "            gotPos, errorCallback, { });\n"
                    + "      }\n"
                    + "      function initiate_watchPosition() {\n"
                    + "        navigator.geolocation.watchPosition(\n"
                    + "            gotPos, errorCallback, { });\n"
                    + "      }\n"
                    + "    </script>\n"
                    + "  </head>\n"
                    + "  <body>\n"
                    + "  </body>\n"
                    + "</html>";

    private static class GrantPermisionAwContentClient extends TestAwContentsClient {
        @Override
        public void onGeolocationPermissionsShowPrompt(
                String origin, AwGeolocationPermissions.Callback callback) {
            callback.invoke(origin, true, true);
        }
    }

    private static class DefaultPermisionAwContentClient extends TestAwContentsClient {
        @Override
        public void onGeolocationPermissionsShowPrompt(
                String origin, AwGeolocationPermissions.Callback callback) {
            // This method is empty intentionally to simulate callback is not referenced.
        }
    }

    private void initAwContents(TestAwContentsClient contentsClient) {
        mContentsClient = contentsClient;
        mAwContents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(() -> mAwContents.getSettings().setGeolocationEnabled(true));
    }

    @Before
    public void setUp() {
        mMockLocationProvider = new MockLocationProvider();
        LocationProviderOverrider.setLocationProviderImpl(mMockLocationProvider);
    }

    @After
    public void tearDown() {
        mMockLocationProvider.stopUpdates();
        mOverriddenFactory = null;
    }

    private int getPositionCountFromJS() {
        int result = -1;
        try {
            result =
                    Integer.parseInt(
                            mActivityTestRule.executeJavaScriptAndWaitForResult(
                                    mAwContents, mContentsClient, "positionCount"));
        } catch (Exception e) {
            throw new AssertionError("Unable to get positionCount", e);
        }
        return result;
    }

    private void ensureGeolocationRunning(final boolean running) {
        AwActivityTestRule.pollInstrumentationThread(
                () -> mMockLocationProvider.isRunning() == running);
    }

    private static class GeolocationOnInsecureOriginsTestDependencyFactory
            extends TestDependencyFactory {
        private boolean mAllow;

        public GeolocationOnInsecureOriginsTestDependencyFactory(boolean allow) {
            mAllow = allow;
        }

        @Override
        public AwSettings createAwSettings(Context context, boolean supportLegacyQuirks) {
            return new AwSettings(
                    context,
                    /* isAccessFromFileURLsGrantedByDefault= */ false,
                    supportLegacyQuirks,
                    /* allowEmptyDocumentPersistence= */ false,
                    mAllow,
                    /* doNotUpdateSelectionOnMutatingSelectionRange= */ false);
        }
    }

    /** Ensure that a call to navigator.getCurrentPosition works in WebView. */
    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testGetPosition() throws Throwable {
        initAwContents(new GrantPermisionAwContentClient());
        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML,
                "text/html",
                false,
                "https://google.com/",
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mAwContents.evaluateJavaScriptForTests(
                                "initiate_getCurrentPosition();", null));

        AwActivityTestRule.pollInstrumentationThread(() -> getPositionCountFromJS() == 1);

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mAwContents.evaluateJavaScriptForTests(
                                "initiate_getCurrentPosition();", null));
        AwActivityTestRule.pollInstrumentationThread(() -> getPositionCountFromJS() == 2);
    }

    /** Ensure that a call to navigator.watchPosition works in WebView. */
    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testWatchPosition() throws Throwable {
        initAwContents(new GrantPermisionAwContentClient());
        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML,
                "text/html",
                false,
                "https://google.com/",
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.evaluateJavaScriptForTests("initiate_watchPosition();", null));

        AwActivityTestRule.pollInstrumentationThread(() -> getPositionCountFromJS() > 1);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPauseGeolocationOnPause() throws Throwable {
        initAwContents(new GrantPermisionAwContentClient());
        // Start a watch going.
        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML,
                "text/html",
                false,
                "https://google.com/",
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.evaluateJavaScriptForTests("initiate_watchPosition();", null));

        AwActivityTestRule.pollInstrumentationThread(() -> getPositionCountFromJS() > 1);

        ensureGeolocationRunning(true);

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> mAwContents.onPause());

        ensureGeolocationRunning(false);

        try {
            mActivityTestRule.executeJavaScriptAndWaitForResult(
                    mAwContents, mContentsClient, "positionCount = 0");
        } catch (Exception e) {
            throw new AssertionError("Unable to clear positionCount", e);
        }
        Assert.assertEquals(0, getPositionCountFromJS());

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> mAwContents.onResume());

        ensureGeolocationRunning(true);

        AwActivityTestRule.pollInstrumentationThread(() -> getPositionCountFromJS() > 1);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPauseAwContentsBeforeNavigating() throws Throwable {
        initAwContents(new GrantPermisionAwContentClient());
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> mAwContents.onPause());

        // Start a watch going.
        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML,
                "text/html",
                false,
                "https://google.com/",
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.evaluateJavaScriptForTests("initiate_watchPosition();", null));

        Assert.assertEquals(0, getPositionCountFromJS());

        ensureGeolocationRunning(false);

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> mAwContents.onResume());

        ensureGeolocationRunning(true);

        AwActivityTestRule.pollInstrumentationThread(() -> getPositionCountFromJS() > 1);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testResumeWhenNotStarted() throws Throwable {
        initAwContents(new GrantPermisionAwContentClient());
        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> mAwContents.onPause());

        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML,
                "text/html",
                false,
                "https://google.com/",
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        InstrumentationRegistry.getInstrumentation().runOnMainSync(() -> mAwContents.onResume());

        ensureGeolocationRunning(false);
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testDenyAccessByDefault() throws Throwable {
        initAwContents(new DefaultPermisionAwContentClient());
        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML,
                "text/html",
                false,
                "https://google.com/",
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mAwContents.evaluateJavaScriptForTests(
                                "initiate_getCurrentPosition();", null));

        AwActivityTestRule.pollInstrumentationThread(
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        Runtime.getRuntime().gc();
                        return "deny".equals(mActivityTestRule.getTitleOnUiThread(mAwContents));
                    }
                });
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testDenyOnInsecureOrigins() throws Throwable {
        mOverriddenFactory = new GeolocationOnInsecureOriginsTestDependencyFactory(false);
        initAwContents(new GrantPermisionAwContentClient());
        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML,
                "text/html",
                false,
                "http://google.com/",
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mAwContents.evaluateJavaScriptForTests(
                                "initiate_getCurrentPosition();", null));

        AwActivityTestRule.pollInstrumentationThread(
                new Callable<Boolean>() {
                    @Override
                    public Boolean call() throws Exception {
                        Runtime.getRuntime().gc();
                        return "deny".equals(mActivityTestRule.getTitleOnUiThread(mAwContents));
                    }
                });
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testAllowOnInsecureOriginsByDefault() throws Throwable {
        initAwContents(new GrantPermisionAwContentClient());
        mActivityTestRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                RAW_HTML,
                "text/html",
                false,
                "http://google.com/",
                ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mAwContents.evaluateJavaScriptForTests(
                                "initiate_getCurrentPosition();", null));

        AwActivityTestRule.pollInstrumentationThread(() -> getPositionCountFromJS() > 0);
    }
}
