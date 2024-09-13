// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.net.Uri;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.JsReplyProxy;
import org.chromium.android_webview.WebMessageListener;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.MediaIntegrityApiStatus;
import org.chromium.android_webview.common.MediaIntegrityErrorCode;
import org.chromium.android_webview.common.MediaIntegrityErrorWrapper;
import org.chromium.android_webview.common.MediaIntegrityProvider;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.PlatformServiceBridgeImpl;
import org.chromium.android_webview.common.ValueOrErrorCallback;
import org.chromium.android_webview.test.AwActivityTestRule.TestDependencyFactory;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.content_public.browser.MessagePayload;
import org.chromium.content_public.browser.MessagePort;
import org.chromium.net.test.util.TestWebServer;

import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.Queue;
import java.util.Set;
import java.util.concurrent.TimeoutException;

/**
 * Test class for the Android Media Integrity API implemented as a Blink extension.
 *
 * <p>This feature requires the older version to be disabled.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class AwMediaIntegrityApiTest extends AwParameterizedTest {

    private static final long CLOUD_PROJECT_NUMBER = 123;
    private static final String CONTENT_BINDING_HASH = "content_binding";
    private static final String UNTRUSTWORTHY_OR_NON_HTTP_HTTPS_ERROR_MESSAGE =
            "NotSupportedError: Failed to execute 'getExperimentalMediaIntegrityTokenProvider' on"
                + " 'WebView': getExperimentalMediaIntegrityTokenProvider: can only be used from"
                + " trustworthy http/https origins";

    @Rule public AwActivityTestRule mRule;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    private final TestWebMessageListener mMessageListener = new TestWebMessageListener();
    private MockPlatformServiceBridge mPlatformBridge;

    public AwMediaIntegrityApiTest(AwSettingsMutation param) {
        mRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Throwable {
        mPlatformBridge = new MockPlatformServiceBridge();
        PlatformServiceBridge.injectInstance(mPlatformBridge);

        mContentsClient = new TestAwContentsClient();
        AwTestContainerView mTestContainerView =
                mRule.createAwTestContainerViewOnMainSync(
                        mContentsClient, false, new TestDependencyFactory());
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        mAwContents.addWebMessageListener(
                                "testListener", new String[] {"*"}, mMessageListener));

        // Ensure API status is ENABLED before the first request.
        mAwContents
                .getSettings()
                .setWebViewIntegrityApiStatus(
                        MediaIntegrityApiStatus.ENABLED, Collections.emptyMap());

        // TODO(crbug.com/330151742): AWMI doesn't use the origin of the base URL set by loads from
        // loadDataWithBaseUrl. For now, use a TestWebServer to load a default HTTPS page.
        try (TestWebServer server = TestWebServer.startSsl()) {
            String url = server.setEmptyResponse("");
            mRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        }
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testApiSurfaceExposed() throws Exception {
        // Check the method name is exposed
        assertJsTruthy("android.webview.getExperimentalMediaIntegrityTokenProvider");

        // Check that the MediaIntegrityTokenProvider class is exposed and has an accessor for the
        // cloudProjectNumber
        assertJsTruthy("android.webview.MediaIntegrityTokenProvider");
        assertJsTruthy(
                "Object.hasOwn(android.webview.MediaIntegrityTokenProvider.prototype,"
                        + " \"cloudProjectNumber\")");

        // Check that the MediaIntegrityError is exposed and has a mediaIntegrityErrorName property.
        assertJsTruthy("android.webview.MediaIntegrityError");
        assertJsTruthy(
                "Object.hasOwn(android.webview.MediaIntegrityError.prototype,"
                        + " \"mediaIntegrityErrorName\")");
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "disable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testApiSurfaceNotExposedWhenFeatureDisabled() throws Exception {
        assertJsTruthy("!('android' in window)");
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testProviderGetterNotExposedForDataUris() throws Throwable {
        mRule.loadDataSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), "", "text/html", false);
        assertNotExposed();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testProviderGetterExposedButRejectedForDataUrisWithHttpsBaseUrls()
            throws Throwable {
        // An HTTPS base URL has a secure context, unlike a plain data URL. This exposes the API,
        // but we deliberately reject the data URL in the implementation.
        mRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                "",
                "text/html",
                false,
                "https://example.com/",
                null);
        final String testScript =
                getTestScript(CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH));
        Assert.assertEquals(
                getExpectedErrorMessage(MediaIntegrityErrorCode.NON_RECOVERABLE_ERROR),
                runTestScriptAndWaitForResult(testScript));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testProviderGetterNotExposedForAboutBlank() throws Throwable {
        mRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), "about:blank");
        assertNotExposed();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testProviderGetterNotExposedForPlaintextHttp() throws Throwable {
        mRule.loadDataWithBaseUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                "",
                "text/html",
                false,
                "http://example.com/",
                null);
        assertNotExposed();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testProviderGetterUseableForLocalhostHttp() throws Throwable {
        try (TestWebServer server = TestWebServer.start()) {
            String url = server.setEmptyResponse("");
            mRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        }
        String mockToken = "abc123def456";
        MockTokenProvider mockTokenProvider = new MockTokenProvider();
        mockTokenProvider.addRequestToken(CONTENT_BINDING_HASH, mockToken);

        mPlatformBridge.addProviderResponse(
                CLOUD_PROJECT_NUMBER, MediaIntegrityApiStatus.ENABLED, mockTokenProvider);

        String actualToken =
                runTestScriptAndWaitForResult(
                        getTestScript(
                                CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH)));
        Assert.assertEquals(mockToken, actualToken);

        // Assert that the token manager was instantiated with the correct cloud project number
        Assert.assertEquals(1, mPlatformBridge.getTotalProviderCallCount());
        Assert.assertEquals(
                1,
                mPlatformBridge.getProviderCallCount(
                        CLOUD_PROJECT_NUMBER, MediaIntegrityApiStatus.ENABLED));

        // Assert that the content binding hash was passed to the TokenProvider
        Assert.assertEquals(1, mockTokenProvider.getTotalCallCount());
        Assert.assertEquals(1, mockTokenProvider.getCallCount(CONTENT_BINDING_HASH));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testProviderGetterExposedButRejectedForFileUris() throws Throwable {
        mRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                "file:///android_asset/hello.html");
        final String testScript =
                getTestScript(CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH));
        Assert.assertEquals(
                UNTRUSTWORTHY_OR_NON_HTTP_HTTPS_ERROR_MESSAGE,
                runTestScriptAndWaitForResult(testScript));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testProviderGetterExposedButRejectedForContentUris() throws Throwable {
        final String testHtmlContentPath = "hello.html";
        final String testHtmlContent =
                "<!DOCTYPE html><html><body>Hello. I'm from a content-provider.</body></html>";
        TestContentProvider.register(
                testHtmlContentPath, "text/html", testHtmlContent.getBytes(StandardCharsets.UTF_8));
        mAwContents.getSettings().setAllowContentAccess(true);
        mRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                TestContentProvider.createContentUrl(testHtmlContentPath));
        final String testScript =
                getTestScript(CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH));
        Assert.assertEquals(
                UNTRUSTWORTHY_OR_NON_HTTP_HTTPS_ERROR_MESSAGE,
                runTestScriptAndWaitForResult(testScript));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testTokenProviderIsNotConstructable() throws Exception {
        // Try to construct a new token provider and turn the error into a string.
        String script =
                """
            let result = "";
            try {
              let provider = new android.webview.MediaIntegrityTokenProvider(123, {});
              result = "unexpected";
            } catch (e) {
              result = "" + e; // Convert to string
            }
            result; // Respond with result.
            """;

        String result =
                mRule.executeJavaScriptAndWaitForResult(mAwContents, mContentsClient, script);
        Assert.assertEquals(
                "\"TypeError: Failed to construct 'MediaIntegrityTokenProvider': Illegal"
                        + " constructor\"",
                result);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testAbleToGetTokenProviderAndToken() throws Exception {
        String mockToken = "abc123def456";
        MockTokenProvider mockTokenProvider = new MockTokenProvider();
        mockTokenProvider.addRequestToken(CONTENT_BINDING_HASH, mockToken);

        mPlatformBridge.addProviderResponse(
                CLOUD_PROJECT_NUMBER, MediaIntegrityApiStatus.ENABLED, mockTokenProvider);

        String actualToken =
                runTestScriptAndWaitForResult(
                        getTestScript(
                                CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH)));
        Assert.assertEquals(mockToken, actualToken);

        // Assert that the token manager was instantiated with the correct cloud project number
        Assert.assertEquals(1, mPlatformBridge.getTotalProviderCallCount());
        Assert.assertEquals(
                1,
                mPlatformBridge.getProviderCallCount(
                        CLOUD_PROJECT_NUMBER, MediaIntegrityApiStatus.ENABLED));

        // Assert that the content binding hash was passed to the TokenProvider
        Assert.assertEquals(1, mockTokenProvider.getTotalCallCount());
        Assert.assertEquals(1, mockTokenProvider.getCallCount(CONTENT_BINDING_HASH));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testErrorWhenAppDisablesApiGlobally() throws Exception {
        mAwContents
                .getSettings()
                .setWebViewIntegrityApiStatus(
                        MediaIntegrityApiStatus.DISABLED, Collections.emptyMap());

        String testScript =
                getTestScript(CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH));

        Assert.assertEquals(
                getExpectedErrorMessage(MediaIntegrityErrorCode.API_DISABLED_BY_APPLICATION),
                runTestScriptAndWaitForResult(testScript));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testErrorWhenAppDisabledApiForUrl() throws Exception {
        String testScript =
                getTestScript(CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH));

        try (TestWebServer server = TestWebServer.startSsl()) {
            String url = server.setEmptyResponse("");
            Map<String, @MediaIntegrityApiStatus Integer> rules =
                    Map.of(url, MediaIntegrityApiStatus.DISABLED);
            mAwContents
                    .getSettings()
                    .setWebViewIntegrityApiStatus(MediaIntegrityApiStatus.ENABLED, rules);
            mRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        }

        Assert.assertEquals(
                getExpectedErrorMessage(MediaIntegrityErrorCode.API_DISABLED_BY_APPLICATION),
                runTestScriptAndWaitForResult(testScript));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testErrorWhenApiDisabledForSourceButEnabledForTopLevel() throws Exception {
        final String result;
        try (final TestWebServer topLevelServer = TestWebServer.startSsl();
                final TestWebServer sourceServer = TestWebServer.startAdditionalSsl()) {
            final String testScript =
                    getTestScript(CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH));
            final String sourceHtml = "<script>" + testScript + "</script>";
            final String sourceUrl =
                    sourceServer.setResponse("/", sourceHtml, Collections.emptyList());
            final String topLevelHtml = "<iframe src=\"" + sourceUrl + "\"></iframe>";
            final String topLevelUrl =
                    topLevelServer.setResponse("/", topLevelHtml, Collections.emptyList());
            final Map<String, @MediaIntegrityApiStatus Integer> rules =
                    Map.of(sourceServer.getResponseUrl(""), MediaIntegrityApiStatus.DISABLED);
            mAwContents
                    .getSettings()
                    .setWebViewIntegrityApiStatus(MediaIntegrityApiStatus.ENABLED, rules);
            final int callCount = mMessageListener.getCurrentCallbackCount();
            mRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), topLevelUrl);
            result = mMessageListener.waitForResponse(callCount);
        }

        Assert.assertEquals(
                getExpectedErrorMessage(MediaIntegrityErrorCode.API_DISABLED_BY_APPLICATION),
                result);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testGetTokenWhenApiEnabledForSourceButDisabledForTopLevel() throws Exception {
        final String mockToken = "abc123def456";
        final MockTokenProvider mockTokenProvider = new MockTokenProvider();
        mockTokenProvider.addRequestToken(CONTENT_BINDING_HASH, mockToken);
        mPlatformBridge.addProviderResponse(
                CLOUD_PROJECT_NUMBER, MediaIntegrityApiStatus.ENABLED, mockTokenProvider);

        final String result;
        try (final TestWebServer topLevelServer = TestWebServer.startSsl();
                final TestWebServer sourceServer = TestWebServer.startAdditionalSsl()) {
            final String testScript =
                    getTestScript(CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH));
            final String sourceHtml = "<script>" + testScript + "</script>";
            final String sourceUrl =
                    sourceServer.setResponse("/", sourceHtml, Collections.emptyList());
            final String topLevelHtml = "<iframe src=\"" + sourceUrl + "\"></iframe>";
            final String topLevelUrl =
                    topLevelServer.setResponse("/", topLevelHtml, Collections.emptyList());
            final Map<String, @MediaIntegrityApiStatus Integer> rules =
                    Map.of(sourceServer.getResponseUrl(""), MediaIntegrityApiStatus.ENABLED);
            mAwContents
                    .getSettings()
                    .setWebViewIntegrityApiStatus(MediaIntegrityApiStatus.DISABLED, rules);
            final int callCount = mMessageListener.getCurrentCallbackCount();
            mRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), topLevelUrl);
            result = mMessageListener.waitForResponse(callCount);
        }

        Assert.assertEquals(mockToken, result);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testCloudProjectNumberAccessibleOnTokenProvider() throws Exception {
        MockTokenProvider mockTokenProvider = new MockTokenProvider();
        mPlatformBridge.addProviderResponse(
                CLOUD_PROJECT_NUMBER, MediaIntegrityApiStatus.ENABLED, mockTokenProvider);

        String script =
                String.format(
                        Locale.ENGLISH,
                        """
                window.android.webview.getExperimentalMediaIntegrityTokenProvider(
                                                                         {cloudProjectNumber: %d})
                    .then(provider => testListener.postMessage("" + provider.cloudProjectNumber))
                    .catch(e => {
                      if (e.mediaIntegrityErrorName !== undefined
                          && e.mediaIntegrityErrorName !== null) {
                        testListener.postMessage(e.mediaIntegrityErrorName);
                      } else {
                        testListener.postMessage("" + e); // Convert error to string for matching.
                      }
                    });
                """,
                        CLOUD_PROJECT_NUMBER);
        String actualCloudProjectNumber = runTestScriptAndWaitForResult(script);
        Assert.assertEquals("" + CLOUD_PROJECT_NUMBER, actualCloudProjectNumber);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testTokenProviderReusedWhenUsingSamePartition() throws Exception {
        String mockToken = "abc123def456";

        MockTokenProvider mockTokenProvider = new MockTokenProvider();
        // Add 2 responses
        mockTokenProvider.addRequestToken(CONTENT_BINDING_HASH, mockToken);
        mockTokenProvider.addRequestToken(CONTENT_BINDING_HASH, mockToken);
        mPlatformBridge.addProviderResponse(
                CLOUD_PROJECT_NUMBER, MediaIntegrityApiStatus.ENABLED, mockTokenProvider);

        String testScript =
                getTestScript(CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH));

        Assert.assertEquals(mockToken, runTestScriptAndWaitForResult(testScript));
        Assert.assertEquals(mockToken, runTestScriptAndWaitForResult(testScript));

        // Assert that the token manager was only instantiated once
        Assert.assertEquals(1, mPlatformBridge.getTotalProviderCallCount());
        Assert.assertEquals(2, mockTokenProvider.getCallCount(CONTENT_BINDING_HASH));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testTokenProviderNotReusedAfterApiModeChange() throws Exception {
        String mockToken = "abc123def456";

        MockTokenProvider mockTokenProvider = new MockTokenProvider();
        mockTokenProvider.addRequestToken(CONTENT_BINDING_HASH, mockToken);
        mPlatformBridge.addProviderResponse(
                CLOUD_PROJECT_NUMBER, MediaIntegrityApiStatus.ENABLED, mockTokenProvider);
        MockTokenProvider mockTokenProviderNoAppIdentity = new MockTokenProvider();
        mockTokenProviderNoAppIdentity.addRequestToken(CONTENT_BINDING_HASH, mockToken);
        mPlatformBridge.addProviderResponse(
                CLOUD_PROJECT_NUMBER,
                MediaIntegrityApiStatus.ENABLED_WITHOUT_APP_IDENTITY,
                mockTokenProviderNoAppIdentity);

        String testScript =
                getTestScript(CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH));

        // Ensure API status is ENABLED before the first request
        mAwContents
                .getSettings()
                .setWebViewIntegrityApiStatus(
                        MediaIntegrityApiStatus.ENABLED, Collections.emptyMap());

        Assert.assertEquals(mockToken, runTestScriptAndWaitForResult(testScript));

        // Change the API status
        mAwContents
                .getSettings()
                .setWebViewIntegrityApiStatus(
                        MediaIntegrityApiStatus.ENABLED_WITHOUT_APP_IDENTITY,
                        Collections.emptyMap());

        Assert.assertEquals(mockToken, runTestScriptAndWaitForResult(testScript));

        // Assert that 2 token managers were instantiated, indicating the new API status was used.
        Assert.assertEquals(
                1,
                mPlatformBridge.getProviderCallCount(
                        CLOUD_PROJECT_NUMBER, MediaIntegrityApiStatus.ENABLED));
        Assert.assertEquals(
                1,
                mPlatformBridge.getProviderCallCount(
                        CLOUD_PROJECT_NUMBER,
                        MediaIntegrityApiStatus.ENABLED_WITHOUT_APP_IDENTITY));

        Assert.assertEquals(1, mockTokenProvider.getTotalCallCount());
        Assert.assertEquals(1, mockTokenProviderNoAppIdentity.getTotalCallCount());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testTokenProviderNotReusedAcrossDistinctOrigins() throws Exception {
        String mockTokenTestServer1 = "abc123def456";
        String mockTokenTestServer2 = "555555555";

        String contentBindingTestServer1 = "test_server1";
        String contentBindingTestServer2 = "test_server2";

        long cloudProjectNumberTestServer1 = 1234;
        long cloudProjectNumberTestServer2 = 9876;

        MockTokenProvider mockTokenProviderTestServer1 = new MockTokenProvider();
        mockTokenProviderTestServer1.addRequestToken(
                contentBindingTestServer1, mockTokenTestServer1);
        mPlatformBridge.addProviderResponse(
                cloudProjectNumberTestServer1,
                MediaIntegrityApiStatus.ENABLED,
                mockTokenProviderTestServer1);

        MockTokenProvider mockTokenProviderTestServer2 = new MockTokenProvider();
        mockTokenProviderTestServer2.addRequestToken(
                contentBindingTestServer2, mockTokenTestServer2);
        mPlatformBridge.addProviderResponse(
                cloudProjectNumberTestServer2,
                MediaIntegrityApiStatus.ENABLED,
                mockTokenProviderTestServer2);

        try (TestWebServer server1 = TestWebServer.startSsl()) {
            try (TestWebServer server2 = TestWebServer.startAdditionalSsl()) {
                String url1 = server1.setEmptyResponse("/");
                mRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url1);

                Assert.assertEquals(
                        mockTokenTestServer1,
                        runTestScriptAndWaitForResult(
                                getTestScript(
                                        cloudProjectNumberTestServer1,
                                        asStringConstant(contentBindingTestServer1))));

                String url2 = server2.setEmptyResponse("/");
                mRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url2);

                Assert.assertEquals(
                        mockTokenTestServer2,
                        runTestScriptAndWaitForResult(
                                getTestScript(
                                        cloudProjectNumberTestServer2,
                                        asStringConstant(contentBindingTestServer2))));
            }
        }

        // Assert that the token manager was instantiated twice.
        Assert.assertEquals(2, mPlatformBridge.getTotalProviderCallCount());
        // Assert that each token provider was called with the expected content binding.
        Assert.assertEquals(
                1, mockTokenProviderTestServer1.getCallCount(contentBindingTestServer1));
        Assert.assertEquals(
                1, mockTokenProviderTestServer2.getCallCount(contentBindingTestServer2));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testTokenProviderNotReusedAcrossDistinctPartyIFrames() throws Exception {
        String mockTokenA = "abc123def456";
        String mockTokenB = "555555555";

        MockTokenProvider mockTokenProviderA = new MockTokenProvider();
        mockTokenProviderA.addRequestToken(CONTENT_BINDING_HASH, mockTokenA);
        mPlatformBridge.addProviderResponse(
                CLOUD_PROJECT_NUMBER, MediaIntegrityApiStatus.ENABLED, mockTokenProviderA);

        MockTokenProvider mockTokenProviderB = new MockTokenProvider();
        mockTokenProviderB.addRequestToken(CONTENT_BINDING_HASH, mockTokenB);
        mPlatformBridge.addProviderResponse(
                CLOUD_PROJECT_NUMBER, MediaIntegrityApiStatus.ENABLED, mockTokenProviderB);

        try (TestWebServer server = TestWebServer.startSsl();
                TestWebServer thirdPartyServer = TestWebServer.startAdditionalSsl();
                TestWebServer fourthPartyServer = TestWebServer.startAdditionalSsl()) {

            String testScript =
                    getTestScript(CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH));

            String framePage = "<script>" + testScript + "</script>";
            String thirdPartyFrameUrl =
                    thirdPartyServer.setResponse("/frame", framePage, Collections.emptyList());
            String fourthPartyFrameUrl =
                    fourthPartyServer.setResponse("/frame", framePage, Collections.emptyList());

            String mainPage =
                    String.format(
                            """
                  <html>
                  <iframe src="%s"></iframe>
                  <iframe src="%s"></iframe>
                  </html>
                  """,
                            thirdPartyFrameUrl, fourthPartyFrameUrl);

            String url = server.setResponse("/", mainPage, Collections.emptyList());

            int callCount = mMessageListener.getCurrentCallbackCount();
            mRule.loadUrlSync(mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
            List<String> responses = mMessageListener.waitForMultipleResponses(callCount, 2);
            Assert.assertEquals(Set.of(mockTokenA, mockTokenB), Set.copyOf(responses));
        }

        // Assert that the token manager was instantiated twice
        Assert.assertEquals(2, mPlatformBridge.getTotalProviderCallCount());

        Assert.assertEquals(1, mockTokenProviderA.getTotalCallCount());
        Assert.assertEquals(1, mockTokenProviderB.getTotalCallCount());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testTokenProviderNotReusedIfInvalid() throws Exception {

        MockTokenProvider mockTokenProvider1 = new MockTokenProvider();
        mockTokenProvider1.addRequestError(
                CONTENT_BINDING_HASH, MediaIntegrityErrorCode.TOKEN_PROVIDER_INVALID);
        mPlatformBridge.addProviderResponse(
                CLOUD_PROJECT_NUMBER, MediaIntegrityApiStatus.ENABLED, mockTokenProvider1);

        MockTokenProvider mockTokenProvider2 = new MockTokenProvider();
        mockTokenProvider2.addRequestError(
                CONTENT_BINDING_HASH, MediaIntegrityErrorCode.TOKEN_PROVIDER_INVALID);
        mPlatformBridge.addProviderResponse(
                CLOUD_PROJECT_NUMBER, MediaIntegrityApiStatus.ENABLED, mockTokenProvider2);

        String testScript =
                getTestScript(CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH));

        Assert.assertEquals(
                getExpectedErrorMessage(MediaIntegrityErrorCode.TOKEN_PROVIDER_INVALID),
                runTestScriptAndWaitForResult(testScript));

        Assert.assertEquals(
                getExpectedErrorMessage(MediaIntegrityErrorCode.TOKEN_PROVIDER_INVALID),
                runTestScriptAndWaitForResult(testScript));

        // Assert that the integrity manager was called 2 times, indicating that the application
        // is allowed to create a new provider.
        Assert.assertEquals(2, mPlatformBridge.getTotalProviderCallCount());
        Assert.assertEquals(1, mockTokenProvider1.getTotalCallCount());
        Assert.assertEquals(1, mockTokenProvider2.getTotalCallCount());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testTokenRequestAcceptsEmptyString() throws Exception {
        String mockToken = "abc123def456";

        MockTokenProvider mockTokenProvider = new MockTokenProvider();
        mockTokenProvider.addRequestToken("", mockToken);
        mPlatformBridge.addProviderResponse(
                CLOUD_PROJECT_NUMBER, MediaIntegrityApiStatus.ENABLED, mockTokenProvider);

        String testScript = getTestScript(CLOUD_PROJECT_NUMBER, asStringConstant(""));
        String actualResponse = runTestScriptAndWaitForResult(testScript);

        Assert.assertEquals(mockToken, actualResponse);

        Assert.assertEquals(1, mockTokenProvider.getCallCount(""));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testTokenRequestAcceptsNull() throws Exception {
        String mockToken = "abc123def456";

        MockTokenProvider mockTokenProvider = new MockTokenProvider();
        mockTokenProvider.addRequestToken(null, mockToken);
        mPlatformBridge.addProviderResponse(
                CLOUD_PROJECT_NUMBER, MediaIntegrityApiStatus.ENABLED, mockTokenProvider);

        String testScript = getTestScript(CLOUD_PROJECT_NUMBER, "null");
        String actualResponse = runTestScriptAndWaitForResult(testScript);

        Assert.assertEquals(mockToken, actualResponse);

        Assert.assertEquals(1, mockTokenProvider.getCallCount(null));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testTokenRequestAcceptsMissingParameterAsNull() throws Exception {
        String mockToken = "abc123def456";

        MockTokenProvider mockTokenProvider = new MockTokenProvider();
        mockTokenProvider.addRequestToken(null, mockToken);
        mPlatformBridge.addProviderResponse(
                CLOUD_PROJECT_NUMBER, MediaIntegrityApiStatus.ENABLED, mockTokenProvider);

        String testScript = getTestScript(CLOUD_PROJECT_NUMBER, ""); // don't pass any parameter.
        String actualResponse = runTestScriptAndWaitForResult(testScript);

        Assert.assertEquals(mockToken, actualResponse);

        Assert.assertEquals(1, mockTokenProvider.getCallCount(null));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testInvalidCloudProjectNumbersAreRejected() throws Exception {
        // Only numbers up to 2^53-1 can be represented correctly in JavaScript.
        // Test that numbers larger than this are rejected.
        long largeCloudProjectNumber = 1L << 53;

        String testScript = getTestScript(largeCloudProjectNumber, CONTENT_BINDING_HASH);
        String actualResponse = runTestScriptAndWaitForResult(testScript);

        Assert.assertEquals(
                "TypeError: Failed to execute 'getExperimentalMediaIntegrityTokenProvider' on"
                    + " 'WebView': Failed to read the 'cloudProjectNumber' property from"
                    + " 'GetMediaIntegrityTokenProviderParams': Value is outside the 'unsigned long"
                    + " long' value range.",
                actualResponse);

        Assert.assertEquals(0, mPlatformBridge.getTotalProviderCallCount());
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testErrorsMappedGetTokenProvider() throws Exception {
        mPlatformBridge.addProviderError(
                CLOUD_PROJECT_NUMBER,
                MediaIntegrityApiStatus.ENABLED,
                MediaIntegrityErrorCode.INTERNAL_ERROR);
        Assert.assertEquals(
                getExpectedErrorMessage(MediaIntegrityErrorCode.INTERNAL_ERROR),
                runTestScriptAndWaitForResult(
                        getTestScript(
                                CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH))));

        mPlatformBridge.addProviderError(
                CLOUD_PROJECT_NUMBER,
                MediaIntegrityApiStatus.ENABLED,
                MediaIntegrityErrorCode.NON_RECOVERABLE_ERROR);
        Assert.assertEquals(
                getExpectedErrorMessage(MediaIntegrityErrorCode.NON_RECOVERABLE_ERROR),
                runTestScriptAndWaitForResult(
                        getTestScript(
                                CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH))));

        mPlatformBridge.addProviderError(
                CLOUD_PROJECT_NUMBER,
                MediaIntegrityApiStatus.ENABLED,
                MediaIntegrityErrorCode.API_DISABLED_BY_APPLICATION);
        Assert.assertEquals(
                getExpectedErrorMessage(MediaIntegrityErrorCode.API_DISABLED_BY_APPLICATION),
                runTestScriptAndWaitForResult(
                        getTestScript(
                                CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH))));

        mPlatformBridge.addProviderError(
                CLOUD_PROJECT_NUMBER,
                MediaIntegrityApiStatus.ENABLED,
                MediaIntegrityErrorCode.INVALID_ARGUMENT);
        Assert.assertEquals(
                getExpectedErrorMessage(MediaIntegrityErrorCode.INVALID_ARGUMENT),
                runTestScriptAndWaitForResult(
                        getTestScript(
                                CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH))));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testErrorsMappedRequestToken() throws Exception {
        MockTokenProvider mockTokenProvider = new MockTokenProvider();
        mPlatformBridge.addProviderResponse(
                CLOUD_PROJECT_NUMBER, MediaIntegrityApiStatus.ENABLED, mockTokenProvider);

        mockTokenProvider.addRequestError(
                CONTENT_BINDING_HASH, MediaIntegrityErrorCode.INTERNAL_ERROR);
        Assert.assertEquals(
                getExpectedErrorMessage(MediaIntegrityErrorCode.INTERNAL_ERROR),
                runTestScriptAndWaitForResult(
                        getTestScript(
                                CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH))));

        mockTokenProvider.addRequestError(
                CONTENT_BINDING_HASH, MediaIntegrityErrorCode.NON_RECOVERABLE_ERROR);
        Assert.assertEquals(
                getExpectedErrorMessage(MediaIntegrityErrorCode.NON_RECOVERABLE_ERROR),
                runTestScriptAndWaitForResult(
                        getTestScript(
                                CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH))));

        mockTokenProvider.addRequestError(
                CONTENT_BINDING_HASH, MediaIntegrityErrorCode.INVALID_ARGUMENT);
        Assert.assertEquals(
                getExpectedErrorMessage(MediaIntegrityErrorCode.INVALID_ARGUMENT),
                runTestScriptAndWaitForResult(
                        getTestScript(
                                CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH))));

        mockTokenProvider.addRequestError(
                CONTENT_BINDING_HASH, MediaIntegrityErrorCode.TOKEN_PROVIDER_INVALID);
        Assert.assertEquals(
                getExpectedErrorMessage(MediaIntegrityErrorCode.TOKEN_PROVIDER_INVALID),
                runTestScriptAndWaitForResult(
                        getTestScript(
                                CLOUD_PROJECT_NUMBER, asStringConstant(CONTENT_BINDING_HASH))));
    }

    /**
     * Returns a script that creates a token provider for the passed {@code cloudProjectNumber} and
     * then requests a token with the passed {@code contentBinding}.
     */
    @NonNull
    private String getTestScript(long cloudProjectNumber, @NonNull String contentBinding) {
        return String.format(
                Locale.ENGLISH,
                """
            window.android.webview.getExperimentalMediaIntegrityTokenProvider(
                    {cloudProjectNumber: %d}
                ).then(provider => provider.requestToken(%s))
                .then(token => testListener.postMessage(token))
                .catch(e => {
                  if (e.mediaIntegrityErrorName !== undefined
                      && e.mediaIntegrityErrorName !== null) {
                    testListener.postMessage(e.mediaIntegrityErrorName);
                  } else {
                    testListener.postMessage("" + e); // Convert error to string for matching.
                  }
                });
            """,
                cloudProjectNumber,
                contentBinding);
    }

    /** Make the passed string a JS String constant by wrapping in {@code ""}. */
    @NonNull
    private String asStringConstant(@NonNull String value) {
        return String.format("\"%s\"", value);
    }

    @NonNull
    private String runTestScriptAndWaitForResult(@NonNull String script) throws Exception {
        int callCount = mMessageListener.getCurrentCallbackCount();
        mRule.executeJavaScriptAndWaitForResult(mAwContents, mContentsClient, script);
        return mMessageListener.waitForResponse(callCount);
    }

    /** Assert the passed in JavaScript expression evaluates as "truthy". */
    private void assertJsTruthy(@NonNull String jsExpression) throws Exception {
        String result =
                mRule.executeJavaScriptAndWaitForResult(
                        mAwContents,
                        mContentsClient,
                        "!!(" + jsExpression + ") ? \"true\" : \"false\";");
        Assert.assertEquals("\"true\"", result);
    }

    @NonNull
    private String getExpectedErrorMessage(@MediaIntegrityErrorCode int code) {
        return switch (code) {
            case MediaIntegrityErrorCode.INTERNAL_ERROR -> "internal-error";
            case MediaIntegrityErrorCode.NON_RECOVERABLE_ERROR -> "non-recoverable-error";
            case MediaIntegrityErrorCode
                    .API_DISABLED_BY_APPLICATION -> "api-disabled-by-application";
            case MediaIntegrityErrorCode.INVALID_ARGUMENT -> "invalid-argument";
            case MediaIntegrityErrorCode.TOKEN_PROVIDER_INVALID -> "token-provider-invalid";
            default -> {
                Assert.fail("Invalid MediaIntegrityErrorCode: " + code);
                yield "";
            }
        };
    }

    private void assertNotExposed() throws Throwable {
        // Only the getExperimentalMediaIntegrityTokenProvider part of the API gets hidden or not.
        assertJsTruthy(
                "typeof(android.webview.getExperimentalMediaIntegrityTokenProvider) ==="
                        + " 'undefined'");
    }

    /** WebMessageListener that allows us to get async JS responses back for verification. */
    private static class TestWebMessageListener implements WebMessageListener {

        private final CallbackHelper mCallbackHelper = new CallbackHelper();

        private final Queue<String> mResponseQueue = new ArrayDeque<>();

        @Override
        public void onPostMessage(
                MessagePayload payload,
                Uri topLevelOrigin,
                Uri sourceOrigin,
                boolean isMainFrame,
                JsReplyProxy jsReplyProxy,
                MessagePort[] ports) {
            synchronized (mResponseQueue) {
                mResponseQueue.add(payload.getAsString());
            }
            mCallbackHelper.notifyCalled();
        }

        public int getCurrentCallbackCount() {
            return mCallbackHelper.getCallCount();
        }

        @NonNull
        public String waitForResponse(int currentCallCount) throws TimeoutException {
            mCallbackHelper.waitForCallback(currentCallCount);
            synchronized (mResponseQueue) {
                return mResponseQueue.poll();
            }
        }

        @NonNull
        public List<String> waitForMultipleResponses(int currentCallCount, int responsesToWaitFor)
                throws TimeoutException {
            mCallbackHelper.waitForCallback(currentCallCount, responsesToWaitFor);
            List<String> results = new ArrayList<>();
            synchronized (mResponseQueue) {
                for (int i = 0; i < responsesToWaitFor; i++) {
                    results.add(mResponseQueue.poll());
                }
            }
            return results;
        }
    }

    /** Token provider where responses can be queued for testing. */
    private static class MockTokenProvider implements MediaIntegrityProvider {

        private final Map<String, Queue<Object>> mResponses = new HashMap<>();
        private final Map<String, Integer> mCallCounts = new HashMap<>();

        private int mCallCount;

        public int getTotalCallCount() {
            return mCallCount;
        }

        public int getCallCount(@Nullable String contentBinding) {
            //noinspection DataFlowIssue
            return mCallCounts.getOrDefault(contentBinding, 0);
        }

        public void addRequestToken(@Nullable String contentBinding, @NonNull String token) {
            mResponses.computeIfAbsent(contentBinding, s -> new LinkedList<>()).offer(token);
        }

        public void addRequestError(
                @Nullable String contentBinding, @MediaIntegrityErrorCode int errorCode) {
            mResponses
                    .computeIfAbsent(contentBinding, s -> new LinkedList<>())
                    .offer(new MediaIntegrityErrorWrapper(errorCode));
        }

        @Override
        public void requestToken2(
                @Nullable String contentBinding,
                @NonNull ValueOrErrorCallback<String, MediaIntegrityErrorWrapper> callback) {
            mCallCount++;
            Queue<Object> responseQueue = mResponses.get(contentBinding);
            mCallCounts.compute(contentBinding, (s, count) -> count == null ? 1 : count + 1);
            if (responseQueue != null) {
                Object response = responseQueue.poll();
                if (response instanceof String token) {
                    callback.onResult(token);
                    return;
                }
                if (response instanceof MediaIntegrityErrorWrapper error) {
                    callback.onError(error);
                    return;
                }
            }
            Assert.fail("Test was configured without any handlers for input: " + contentBinding);
        }
    }

    /** PlatformServiceBridge where MediaIntegrityProvider responses can be queued. */
    private static class MockPlatformServiceBridge extends PlatformServiceBridgeImpl {

        private static class CallKey {

            long mCloudProjectNumber;
            int mApiStatus;

            public CallKey(long cloudProjectNumber, int apiStatus) {
                mCloudProjectNumber = cloudProjectNumber;
                mApiStatus = apiStatus;
            }

            @Override
            public int hashCode() {
                return Objects.hash(mCloudProjectNumber, mApiStatus);
            }

            @Override
            public boolean equals(@Nullable Object obj) {
                if (obj instanceof CallKey key) {
                    return mCloudProjectNumber == key.mCloudProjectNumber
                            && this.mApiStatus == key.mApiStatus;
                }
                return false;
            }
        }

        int mProviderCallCount;
        private final Map<CallKey, Queue<Object>> mResponses = new HashMap<>();
        private final Map<CallKey, Integer> mCallCounts = new HashMap<>();

        public void addProviderResponse(
                long cloudProjectNumber,
                @MediaIntegrityApiStatus int apiStatus,
                MediaIntegrityProvider provider) {
            CallKey key = new CallKey(cloudProjectNumber, apiStatus);
            mResponses.computeIfAbsent(key, k -> new LinkedList<>()).offer(provider);
        }

        public void addProviderError(
                long cloudProjectNumber,
                @MediaIntegrityApiStatus int apiStatus,
                @MediaIntegrityErrorCode int errorCode) {
            CallKey key = new CallKey(cloudProjectNumber, apiStatus);
            mResponses
                    .computeIfAbsent(key, k -> new LinkedList<>())
                    .offer(new MediaIntegrityErrorWrapper(errorCode));
        }

        public int getProviderCallCount(
                long cloudProjectNumber, @MediaIntegrityApiStatus int apiStatus) {
            //noinspection DataFlowIssue
            return mCallCounts.getOrDefault(new CallKey(cloudProjectNumber, apiStatus), 0);
        }

        public int getTotalProviderCallCount() {
            return mProviderCallCount;
        }

        @Override
        public void getMediaIntegrityProvider2(
                long cloudProjectNumber,
                @MediaIntegrityApiStatus int apiStatus,
                ValueOrErrorCallback<MediaIntegrityProvider, MediaIntegrityErrorWrapper> callback) {
            CallKey key = new CallKey(cloudProjectNumber, apiStatus);
            Queue<Object> responseQueue = mResponses.get(key);
            mCallCounts.compute(key, (callKey, counts) -> counts == null ? 1 : counts + 1);
            mProviderCallCount += 1;

            if (responseQueue != null) {
                Object response = responseQueue.poll();
                if (response instanceof MediaIntegrityProvider provider) {
                    callback.onResult(provider);
                    return;
                }
                if (response instanceof MediaIntegrityErrorWrapper error) {
                    callback.onError(error);
                    return;
                }
            }
            Assert.fail(
                    "Test was configured without any handlers for input: "
                            + cloudProjectNumber
                            + ", "
                            + apiStatus);
        }
    }
}
