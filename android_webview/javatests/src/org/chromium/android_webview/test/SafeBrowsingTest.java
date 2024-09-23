// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertNotEquals;

import android.content.Context;
import android.content.ContextWrapper;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.view.ViewGroup;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContents.DependencyFactory;
import org.chromium.android_webview.AwContents.InternalAccessDelegate;
import org.chromium.android_webview.AwContents.NativeDrawFunctorFactory;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.SafeBrowsingAction;
import org.chromium.android_webview.WebviewErrorCode;
import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingConfigHelper;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingConversionHelper;
import org.chromium.android_webview.safe_browsing.AwSafeBrowsingResponse;
import org.chromium.android_webview.test.TestAwContentsClient.OnReceivedErrorHelper;
import org.chromium.android_webview.test.util.GraphicsTestUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.Feature;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.components.safe_browsing.SafeBrowsingApiHandler;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * Test suite for SafeBrowsing.
 *
 * <p>Ensures that interstitials can be successfully created for malicious pages.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class SafeBrowsingTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    public SafeBrowsingTest(AwSettingsMutation param) {
        mActivityTestRule =
                new AwActivityTestRule(param.getMutation()) {
                    /**
                     * Creates a special BrowserContext that has a safebrowsing api handler which always says
                     * sites are malicious
                     */
                    @Override
                    public AwBrowserContext createAwBrowserContextOnUiThread() {
                        return new MockAwBrowserContext();
                    }
                };
    }

    private SafeBrowsingContentsClient mContentsClient;
    private AwTestContainerView mContainerView;
    private MockAwContents mAwContents;

    private EmbeddedTestServer mTestServer;

    // Used to check which thread a callback is invoked on.
    private volatile boolean mOnUiThread;

    // Used to verify the getSafeBrowsingPrivacyPolicyUrl() API.
    private volatile Uri mPrivacyPolicyUrl;

    // These colors correspond to the body.background attribute in GREEN_HTML_PATH, SAFE_HTML_PATH,
    // MALWARE_HTML_PATH, IFRAME_HTML_PATH, etc. They should only be changed if those values are
    // changed as well
    private static final int GREEN_PAGE_BACKGROUND_COLOR = Color.rgb(0, 255, 0);
    private static final int SAFE_PAGE_BACKGROUND_COLOR = Color.rgb(0, 0, 255);
    private static final int PHISHING_PAGE_BACKGROUND_COLOR = Color.rgb(0, 0, 255);
    private static final int MALWARE_PAGE_BACKGROUND_COLOR = Color.rgb(0, 0, 255);
    private static final int UNWANTED_SOFTWARE_PAGE_BACKGROUND_COLOR = Color.rgb(0, 0, 255);
    private static final int BILLING_PAGE_BACKGROUND_COLOR = Color.rgb(0, 0, 255);
    private static final int IFRAME_EMBEDDER_BACKGROUND_COLOR = Color.rgb(10, 10, 10);

    private static final String RESOURCE_PATH = "/android_webview/test/data";

    // A blank green page
    private static final String GREEN_HTML_PATH = RESOURCE_PATH + "/green.html";

    // Blank blue pages
    private static final String SAFE_HTML_PATH = RESOURCE_PATH + "/safe.html";
    private static final String PHISHING_HTML_PATH = RESOURCE_PATH + "/phishing.html";
    private static final String MALWARE_HTML_PATH = RESOURCE_PATH + "/malware.html";
    private static final String MALWARE_WITH_IMAGE_HTML_PATH =
            RESOURCE_PATH + "/malware_with_image.html";
    private static final String UNWANTED_SOFTWARE_HTML_PATH =
            RESOURCE_PATH + "/unwanted_software.html";
    private static final String BILLING_HTML_PATH = RESOURCE_PATH + "/billing.html";

    // A gray page with an iframe to MALWARE_HTML_PATH
    private static final String IFRAME_HTML_PATH = RESOURCE_PATH + "/iframe.html";

    // These URLs will be CTS-tested and should not be changed.
    private static final String WEB_UI_MALWARE_URL = "chrome://safe-browsing/match?type=malware";
    private static final String WEB_UI_PHISHING_URL = "chrome://safe-browsing/match?type=phishing";
    private static final String WEB_UI_HOST = "safe-browsing";

    /**
     * A fake SafeBrowsingApiHandler which treats URLs ending in certain HTML paths as malicious
     * URLs that should be blocked.
     */
    public static class MockSafeBrowsingApiHandler implements SafeBrowsingApiHandler {
        private SafeBrowsingApiHandler.Observer mObserver;
        // Corresponding to SafeBrowsingResponseStatus.SUCCESS_WITH_LOCAL_BLOCKLIST
        private static final int RESPONSE_STATUS_SUCCESS_WITH_LOCAL_BLOCK_LIST = 0;
        private static final int NO_THREAT = 0;
        private static final int PHISHING_CODE = 2;
        private static final int MALWARE_CODE = 4;
        private static final int UNWANTED_SOFTWARE_CODE = 3;
        private static final int BILLING_CODE = 15;

        // Mock time it takes for a lookup request to complete.
        private static final long CHECK_DELTA_US = 10;

        @Override
        public void startUriLookup(long callbackId, String uri, int[] threatTypes, int protocol) {
            final int detectedType;
            Arrays.sort(threatTypes);
            if (uri.endsWith(PHISHING_HTML_PATH)
                    && Arrays.binarySearch(threatTypes, PHISHING_CODE) >= 0) {
                detectedType = PHISHING_CODE;
            } else if (uri.endsWith(MALWARE_HTML_PATH)
                    && Arrays.binarySearch(threatTypes, MALWARE_CODE) >= 0) {
                detectedType = MALWARE_CODE;
            } else if (uri.endsWith(UNWANTED_SOFTWARE_HTML_PATH)
                    && Arrays.binarySearch(threatTypes, UNWANTED_SOFTWARE_CODE) >= 0) {
                detectedType = UNWANTED_SOFTWARE_CODE;
            } else if (uri.endsWith(BILLING_HTML_PATH)
                    && Arrays.binarySearch(threatTypes, BILLING_CODE) >= 0) {
                detectedType = BILLING_CODE;
            } else {
                detectedType = NO_THREAT;
            }
            PostTask.runOrPostTask(
                    TaskTraits.UI_DEFAULT,
                    (Runnable)
                            () ->
                                    mObserver.onUrlCheckDone(
                                            callbackId,
                                            LookupResult.SUCCESS,
                                            detectedType,
                                            /* threatAttributes= */ new int[0],
                                            RESPONSE_STATUS_SUCCESS_WITH_LOCAL_BLOCK_LIST,
                                            CHECK_DELTA_US));
        }

        @Override
        public void setObserver(Observer observer) {
            mObserver = observer;
        }
    }

    /**
     * A fake PlatformServiceBridge that allows tests to make safe browsing requests without GMS.
     */
    private static class MockPlatformServiceBridge extends PlatformServiceBridge {
        @Override
        public boolean canUseGms() {
            return true;
        }
    }

    /**
     * A fake AwBrowserContext which loads the MockSafeBrowsingApiHandler instead of the real one.
     */
    private static class MockAwBrowserContext extends AwBrowserContext {
        public MockAwBrowserContext() {
            super(0);
            SafeBrowsingApiBridge.setSafeBrowsingApiHandler(new MockSafeBrowsingApiHandler());
        }
    }

    private static class MockAwContents extends TestAwContents {
        private boolean mCanShowInterstitial;
        private boolean mCanShowBigInterstitial;

        public MockAwContents(
                AwBrowserContext browserContext,
                ViewGroup containerView,
                Context context,
                InternalAccessDelegate internalAccessAdapter,
                NativeDrawFunctorFactory nativeDrawFunctorFactory,
                AwContentsClient contentsClient,
                AwSettings settings,
                DependencyFactory dependencyFactory) {
            super(
                    browserContext,
                    containerView,
                    context,
                    internalAccessAdapter,
                    nativeDrawFunctorFactory,
                    contentsClient,
                    settings,
                    dependencyFactory);
            mCanShowInterstitial = true;
            mCanShowBigInterstitial = true;
        }

        public void setCanShowInterstitial(boolean able) {
            mCanShowInterstitial = able;
        }

        public void setCanShowBigInterstitial(boolean able) {
            mCanShowBigInterstitial = able;
        }

        @Override
        protected boolean canShowInterstitial() {
            return mCanShowInterstitial;
        }

        @Override
        protected boolean canShowBigInterstitial() {
            return mCanShowBigInterstitial;
        }
    }

    /** An AwContentsClient with customizable behavior for onSafeBrowsingHit(). */
    private static class SafeBrowsingContentsClient extends TestAwContentsClient {
        private AwWebResourceRequest mLastRequest;
        private int mLastThreatType;
        private int mAction = SafeBrowsingAction.SHOW_INTERSTITIAL;
        private int mOnSafeBrowsingHitCount;
        private boolean mReporting = true;

        @Override
        public void onSafeBrowsingHit(
                AwWebResourceRequest request,
                int threatType,
                Callback<AwSafeBrowsingResponse> callback) {
            mLastRequest = request;
            mLastThreatType = threatType;
            mOnSafeBrowsingHitCount++;
            callback.onResult(new AwSafeBrowsingResponse(mAction, mReporting));
        }

        public AwWebResourceRequest getLastRequest() {
            return mLastRequest;
        }

        public int getLastThreatType() {
            return mLastThreatType;
        }

        public int getOnSafeBrowsingHitCount() {
            return mOnSafeBrowsingHitCount;
        }

        public void setSafeBrowsingAction(int action) {
            mAction = action;
        }

        public void setReporting(boolean value) {
            mReporting = value;
        }
    }

    private static class SafeBrowsingDependencyFactory
            extends AwActivityTestRule.TestDependencyFactory {
        @Override
        public AwContents createAwContents(
                AwBrowserContext browserContext,
                ViewGroup containerView,
                Context context,
                InternalAccessDelegate internalAccessAdapter,
                NativeDrawFunctorFactory nativeDrawFunctorFactory,
                AwContentsClient contentsClient,
                AwSettings settings,
                DependencyFactory dependencyFactory) {
            return new MockAwContents(
                    browserContext,
                    containerView,
                    context,
                    internalAccessAdapter,
                    nativeDrawFunctorFactory,
                    contentsClient,
                    settings,
                    dependencyFactory);
        }
    }

    private static class AllowlistHelper extends CallbackHelper implements Callback<Boolean> {
        public boolean success;

        @Override
        public void onResult(Boolean success) {
            this.success = success;
            notifyCalled();
        }
    }

    @Before
    public void setUp() {
        mContentsClient = new SafeBrowsingContentsClient();
        mContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(
                        mContentsClient, false, new SafeBrowsingDependencyFactory());
        mAwContents = (MockAwContents) mContainerView.getAwContents();

        MockPlatformServiceBridge mockPlatformServiceBridge = new MockPlatformServiceBridge();
        PlatformServiceBridge.injectInstance(mockPlatformServiceBridge);

        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());

        // Need to configure user opt-in, otherwise WebView won't perform Safe Browsing checks.
        AwSafeBrowsingConfigHelper.setSafeBrowsingUserOptIn(true);

        // Some tests need to inject JavaScript.
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
    }

    private int getPageColor() {
        Bitmap bitmap =
                GraphicsTestUtils.drawAwContentsOnUiThread(
                        mAwContents, mContainerView.getWidth(), mContainerView.getHeight());
        return bitmap.getPixel(0, 0);
    }

    private void loadGreenPage() throws Exception {
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                mTestServer.getURL(GREEN_HTML_PATH));

        // Make sure we actually wait for the page to be visible
        mActivityTestRule.waitForVisualStateCallback(mAwContents);
    }

    private void waitForInterstitialDomToLoad() {
        final String script = "document.readyState;";
        final String expected = "\"complete\"";

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        Criteria.checkThat(
                                mActivityTestRule.executeJavaScriptAndWaitForResult(
                                        mAwContents, mContentsClient, script),
                                Matchers.is(expected));
                    } catch (Exception e) {
                        throw new RuntimeException(e);
                    }
                });
    }

    private void clickBackToSafety() throws Exception {
        clickLinkById("primary-button");
    }

    private void clickVisitUnsafePageQuietInterstitial() throws Exception {
        clickLinkById("details-link");
        clickLinkById("proceed-link");
    }

    private void clickVisitUnsafePage() throws Exception {
        clickLinkById("details-button");
        clickLinkById("proceed-link");
    }

    private void clickLinkById(String id) throws Exception {
        final String script = "document.getElementById('" + id + "').click();";
        mActivityTestRule.executeJavaScriptAndWaitForResult(mAwContents, mContentsClient, script);
    }

    private void loadPathAndWaitForInterstitial(final String path) throws Exception {
        loadPathAndWaitForInterstitial(path, /* waitForVisualStateCallback= */ true);
    }

    /**
     * waitForVisualStateCallback should be false for tests where the subresource triggers the
     * SafeBrowsing check. See crbug.com/1107540 for details.
     */
    private void loadPathAndWaitForInterstitial(
            final String path, boolean waitForVisualStateCallback) throws Exception {
        final String responseUrl = mTestServer.getURL(path);
        mActivityTestRule.loadUrlAsync(mAwContents, responseUrl);
        // Subresource triggered interstitials will trigger after the page containing the
        // subresource has loaded (and displayed), so we first wait for the interstitial to be
        // triggered, then for a visual state callback to allow the interstitial to render.
        CriteriaHelper.pollUiThread(() -> mAwContents.isDisplayingInterstitialForTesting());
        // Wait for the interstitial to actually render.
        if (waitForVisualStateCallback) {
            mActivityTestRule.waitForVisualStateCallback(mAwContents);
        }
    }

    private void assertTargetPageHasLoaded(int pageColor) throws Exception {
        mActivityTestRule.waitForVisualStateCallback(mAwContents);
        Assert.assertEquals(
                "Target page should be visible",
                colorToString(pageColor),
                colorToString(
                        GraphicsTestUtils.getPixelColorAtCenterOfView(
                                mAwContents, mContainerView)));
    }

    private void assertGreenPageShowing() {
        Assert.assertEquals(
                "Original page should be showing",
                colorToString(GREEN_PAGE_BACKGROUND_COLOR),
                colorToString(
                        GraphicsTestUtils.getPixelColorAtCenterOfView(
                                mAwContents, mContainerView)));
    }

    private void assertGreenPageNotShowing() {
        assertNotEquals(
                "Original page should not be showing",
                colorToString(GREEN_PAGE_BACKGROUND_COLOR),
                colorToString(
                        GraphicsTestUtils.getPixelColorAtCenterOfView(
                                mAwContents, mContainerView)));
    }

    private void assertTargetPageNotShowing(int pageColor) {
        assertNotEquals(
                "Target page should not be showing",
                colorToString(pageColor),
                colorToString(
                        GraphicsTestUtils.getPixelColorAtCenterOfView(
                                mAwContents, mContainerView)));
    }

    /**
     * Converts a color from the confusing integer representation to a more readable string
     * respresentation. There is a 1:1 mapping between integer and string representations, so it's
     * valid to compare strings directly. The string representation is better for assert output.
     *
     * @param color integer representation of the color
     * @return a String representation of the color in RGBA format
     */
    private String colorToString(int color) {
        return "("
                + Color.red(color)
                + ","
                + Color.green(color)
                + ","
                + Color.blue(color)
                + ","
                + Color.alpha(color)
                + ")";
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingGetterAndSetter() throws Throwable {
        Assert.assertTrue(
                "Getter API should follow manifest tag by default",
                mActivityTestRule.getAwSettingsOnUiThread(mAwContents).getSafeBrowsingEnabled());
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setSafeBrowsingEnabled(false);
        Assert.assertFalse(
                "setSafeBrowsingEnabled(false) should change the getter",
                mActivityTestRule.getAwSettingsOnUiThread(mAwContents).getSafeBrowsingEnabled());
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setSafeBrowsingEnabled(true);
        Assert.assertTrue(
                "setSafeBrowsingEnabled(true) should change the getter",
                mActivityTestRule.getAwSettingsOnUiThread(mAwContents).getSafeBrowsingEnabled());
        AwSafeBrowsingConfigHelper.setSafeBrowsingUserOptIn(false);
        Assert.assertTrue(
                "Getter API should ignore user opt-out",
                mActivityTestRule.getAwSettingsOnUiThread(mAwContents).getSafeBrowsingEnabled());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingDoesNotBlockSafePages() throws Throwable {
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(SAFE_HTML_PATH);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(SAFE_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingBlocksUnwantedSoftwarePages() throws Throwable {
        loadGreenPage();
        loadPathAndWaitForInterstitial(UNWANTED_SOFTWARE_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(UNWANTED_SOFTWARE_PAGE_BACKGROUND_COLOR);
        // Assume that we are rendering the interstitial, since we see neither the previous page nor
        // the target page
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingBlocksBillingPages() throws Throwable {
        loadGreenPage();
        loadPathAndWaitForInterstitial(BILLING_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(BILLING_PAGE_BACKGROUND_COLOR);
        // Assume that we are rendering the interstitial, since we see neither the previous page nor
        // the target page
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingOnSafeBrowsingHitBillingCode() throws Throwable {
        loadGreenPage();
        loadPathAndWaitForInterstitial(BILLING_HTML_PATH);

        // Check onSafeBrowsingHit arguments
        final String responseUrl = mTestServer.getURL(BILLING_HTML_PATH);
        Assert.assertEquals(responseUrl, mContentsClient.getLastRequest().url);
        // The expectedCode intentionally depends on targetSdk (and is disconnected from SDK_INT).
        // This is for backwards compatibility with apps with a lower targetSdk.
        int expectedCode =
                ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion
                                >= Build.VERSION_CODES.Q
                        ? AwSafeBrowsingConversionHelper.SAFE_BROWSING_THREAT_BILLING
                        : AwSafeBrowsingConversionHelper.SAFE_BROWSING_THREAT_UNKNOWN;
        Assert.assertEquals(expectedCode, mContentsClient.getLastThreatType());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingBlocksPhishingPages() throws Throwable {
        loadGreenPage();
        loadPathAndWaitForInterstitial(PHISHING_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(PHISHING_PAGE_BACKGROUND_COLOR);
        // Assume that we are rendering the interstitial, since we see neither the previous page nor
        // the target page
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingAllowlistedUnsafePagesDontShowInterstitial() throws Throwable {
        int onSafeBrowsingCount = mContentsClient.getOnSafeBrowsingHitCount();
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        verifyAllowlistRule(Uri.parse(responseUrl).getHost(), true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(MALWARE_PAGE_BACKGROUND_COLOR);
        Assert.assertEquals(
                "onSafeBrowsingHit count should not be changed by allowed URLs",
                onSafeBrowsingCount,
                mContentsClient.getOnSafeBrowsingHitCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingAllowlistHardcodedWebUiPages() throws Throwable {
        int onSafeBrowsingCount = mContentsClient.getOnSafeBrowsingHitCount();
        loadGreenPage();
        verifyAllowlistRule(WEB_UI_HOST, true);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), WEB_UI_MALWARE_URL);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), WEB_UI_PHISHING_URL);
        Assert.assertEquals(
                "onSafeBrowsingHit count should not be changed by allowed URLs",
                onSafeBrowsingCount,
                mContentsClient.getOnSafeBrowsingHitCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingAllowlistHardcodedWebUiPageBackToSafety() throws Throwable {
        mContentsClient.setSafeBrowsingAction(SafeBrowsingAction.BACK_TO_SAFETY);

        loadGreenPage();
        OnReceivedErrorHelper errorHelper = mContentsClient.getOnReceivedErrorHelper();
        int errorCount = errorHelper.getCallCount();
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), WEB_UI_MALWARE_URL);
        errorHelper.waitForCallback(errorCount);
        Assert.assertEquals(
                WebviewErrorCode.ERROR_UNSAFE_RESOURCE, errorHelper.getError().errorCode);
        Assert.assertEquals(
                "Network error is for the malicious page",
                WEB_UI_MALWARE_URL,
                errorHelper.getRequest().url);

        assertGreenPageShowing();

        // Check onSafeBrowsingHit arguments
        Assert.assertEquals(WEB_UI_MALWARE_URL, mContentsClient.getLastRequest().url);
        Assert.assertEquals(
                AwSafeBrowsingConversionHelper.SAFE_BROWSING_THREAT_MALWARE,
                mContentsClient.getLastThreatType());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCallbackCalledOnSafeBrowsingBadAllowlistRule() throws Throwable {
        verifyAllowlistRule("http://www.google.com", false);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testCallbackCalledOnSafeBrowsingGoodAllowlistRule() throws Throwable {
        verifyAllowlistRule("www.google.com", true);
    }

    private void verifyAllowlistRule(final String rule, boolean expected) throws Throwable {
        final AllowlistHelper helper = new AllowlistHelper();
        final int count = helper.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ArrayList<String> s = new ArrayList<String>();
                    s.add(rule);
                    AwContentsStatics.setSafeBrowsingAllowlist(s, helper);
                });
        helper.waitForCallback(count);
        Assert.assertEquals(expected, helper.success);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingShowsInterstitialForMainFrame() throws Throwable {
        loadGreenPage();
        loadPathAndWaitForInterstitial(MALWARE_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(MALWARE_PAGE_BACKGROUND_COLOR);
        // Assume that we are rendering the interstitial, since we see neither the previous page
        // nor the target page
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingNoInterstitialForSubresource() throws Throwable {
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(IFRAME_HTML_PATH);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(IFRAME_EMBEDDER_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingProceedThroughInterstitialForMainFrame() throws Throwable {
        int pageFinishedCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        loadPathAndWaitForInterstitial(MALWARE_HTML_PATH);
        waitForInterstitialDomToLoad();
        int onSafeBrowsingCountBeforeClick = mContentsClient.getOnSafeBrowsingHitCount();
        clickVisitUnsafePage();
        mContentsClient.getOnPageFinishedHelper().waitForCallback(pageFinishedCount);
        assertTargetPageHasLoaded(MALWARE_PAGE_BACKGROUND_COLOR);
        // Check there is not an extra onSafeBrowsingHit call after proceeding.
        Assert.assertEquals(
                onSafeBrowsingCountBeforeClick, mContentsClient.getOnSafeBrowsingHitCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingDontProceedCausesNetworkErrorForMainFrame() throws Throwable {
        loadGreenPage();
        loadPathAndWaitForInterstitial(MALWARE_HTML_PATH);
        OnReceivedErrorHelper errorHelper = mContentsClient.getOnReceivedErrorHelper();
        int errorCount = errorHelper.getCallCount();
        waitForInterstitialDomToLoad();
        clickBackToSafety();
        errorHelper.waitForCallback(errorCount);
        Assert.assertEquals(
                WebviewErrorCode.ERROR_UNSAFE_RESOURCE, errorHelper.getError().errorCode);
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        Assert.assertEquals(
                "Network error is for the malicious page",
                responseUrl,
                errorHelper.getRequest().url);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingDontProceedNavigatesBackForMainFrame() throws Throwable {
        loadGreenPage();
        loadPathAndWaitForInterstitial(MALWARE_HTML_PATH);
        waitForInterstitialDomToLoad();
        OnReceivedErrorHelper errorHelper = mContentsClient.getOnReceivedErrorHelper();
        int errorCount = errorHelper.getCallCount();
        clickBackToSafety();
        errorHelper.waitForCallback(errorCount);
        mActivityTestRule.waitForVisualStateCallback(mAwContents);
        assertTargetPageNotShowing(MALWARE_PAGE_BACKGROUND_COLOR);
        assertGreenPageShowing();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingCanBeDisabledPerWebview() throws Throwable {
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setSafeBrowsingEnabled(false);

        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(MALWARE_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingCanBeDisabledPerWebview_withImage() throws Throwable {
        // In particular this test checks that there is no crash when network service
        // is enabled, safebrowsing is disabled and the RendererURLLoaderThrottle
        // attempts to check a url through loading an image (crbug.com/889479).
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setSafeBrowsingEnabled(false);

        final String responseUrl = mTestServer.getURL(MALWARE_WITH_IMAGE_HTML_PATH);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(MALWARE_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    @CommandLineFlags.Add(AwSwitches.WEBVIEW_DISABLE_SAFEBROWSING_SUPPORT)
    public void testSafeBrowsingCanBeEnabledPerWebview() throws Throwable {
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(MALWARE_PAGE_BACKGROUND_COLOR);

        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setSafeBrowsingEnabled(true);

        loadGreenPage();
        loadPathAndWaitForInterstitial(MALWARE_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(MALWARE_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingShowsNetworkErrorForInvisibleViews() throws Throwable {
        mAwContents.setCanShowInterstitial(false);
        mAwContents.setCanShowBigInterstitial(false);
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        OnReceivedErrorHelper errorHelper = mContentsClient.getOnReceivedErrorHelper();
        int errorCount = errorHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, responseUrl);
        errorHelper.waitForCallback(errorCount);
        Assert.assertEquals(
                WebviewErrorCode.ERROR_UNSAFE_RESOURCE, errorHelper.getError().errorCode);
        Assert.assertEquals(
                "Network error is for the malicious page",
                responseUrl,
                errorHelper.getRequest().url);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingShowsQuietInterstitialForOddSizedViews() throws Throwable {
        mAwContents.setCanShowBigInterstitial(false);
        loadGreenPage();
        loadPathAndWaitForInterstitial(MALWARE_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(MALWARE_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingCanShowQuietPhishingInterstitial() throws Throwable {
        mAwContents.setCanShowBigInterstitial(false);
        loadGreenPage();
        loadPathAndWaitForInterstitial(PHISHING_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(PHISHING_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingCanShowQuietUnwantedSoftwareInterstitial() throws Throwable {
        mAwContents.setCanShowBigInterstitial(false);
        loadGreenPage();
        loadPathAndWaitForInterstitial(UNWANTED_SOFTWARE_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(UNWANTED_SOFTWARE_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingCanShowQuietBillingInterstitial() throws Throwable {
        mAwContents.setCanShowBigInterstitial(false);
        loadGreenPage();
        loadPathAndWaitForInterstitial(BILLING_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(BILLING_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingProceedQuietInterstitial() throws Throwable {
        mAwContents.setCanShowBigInterstitial(false);
        int pageFinishedCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        loadPathAndWaitForInterstitial(PHISHING_HTML_PATH);
        waitForInterstitialDomToLoad();
        clickVisitUnsafePageQuietInterstitial();
        mContentsClient.getOnPageFinishedHelper().waitForCallback(pageFinishedCount);
        assertTargetPageHasLoaded(PHISHING_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingOnSafeBrowsingHitShowInterstitial() throws Throwable {
        mContentsClient.setSafeBrowsingAction(SafeBrowsingAction.SHOW_INTERSTITIAL);

        loadGreenPage();
        loadPathAndWaitForInterstitial(PHISHING_HTML_PATH);
        assertGreenPageNotShowing();
        assertTargetPageNotShowing(PHISHING_PAGE_BACKGROUND_COLOR);
        // Assume that we are rendering the interstitial, since we see neither the previous page nor
        // the target page

        // Check onSafeBrowsingHit arguments
        final String responseUrl = mTestServer.getURL(PHISHING_HTML_PATH);
        Assert.assertEquals(responseUrl, mContentsClient.getLastRequest().url);
        Assert.assertEquals(
                AwSafeBrowsingConversionHelper.SAFE_BROWSING_THREAT_PHISHING,
                mContentsClient.getLastThreatType());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingOnSafeBrowsingHitProceed() throws Throwable {
        mContentsClient.setSafeBrowsingAction(SafeBrowsingAction.PROCEED);

        loadGreenPage();
        final String responseUrl = mTestServer.getURL(PHISHING_HTML_PATH);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        mActivityTestRule.waitForVisualStateCallback(mAwContents);
        assertTargetPageHasLoaded(PHISHING_PAGE_BACKGROUND_COLOR);

        // Check onSafeBrowsingHit arguments
        Assert.assertEquals(responseUrl, mContentsClient.getLastRequest().url);
        Assert.assertEquals(
                AwSafeBrowsingConversionHelper.SAFE_BROWSING_THREAT_PHISHING,
                mContentsClient.getLastThreatType());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingOnSafeBrowsingHitBackToSafety() throws Throwable {
        mContentsClient.setSafeBrowsingAction(SafeBrowsingAction.BACK_TO_SAFETY);

        loadGreenPage();
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        OnReceivedErrorHelper errorHelper = mContentsClient.getOnReceivedErrorHelper();
        int errorCount = errorHelper.getCallCount();
        mActivityTestRule.loadUrlAsync(mAwContents, responseUrl);
        errorHelper.waitForCallback(errorCount);
        Assert.assertEquals(
                WebviewErrorCode.ERROR_UNSAFE_RESOURCE, errorHelper.getError().errorCode);
        Assert.assertEquals(
                "Network error is for the malicious page",
                responseUrl,
                errorHelper.getRequest().url);

        assertGreenPageShowing();

        // Check onSafeBrowsingHit arguments
        Assert.assertEquals(responseUrl, mContentsClient.getLastRequest().url);
        Assert.assertEquals(
                AwSafeBrowsingConversionHelper.SAFE_BROWSING_THREAT_MALWARE,
                mContentsClient.getLastThreatType());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingOnSafeBrowsingHitHideReportingCheckbox() throws Throwable {
        mContentsClient.setReporting(false);
        loadGreenPage();
        loadPathAndWaitForInterstitial(PHISHING_HTML_PATH);
        waitForInterstitialDomToLoad();

        Assert.assertFalse(getVisibilityByIdOnInterstitial("extended-reporting-opt-in"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingReportingCheckboxVisibleByDefault() throws Throwable {
        loadGreenPage();
        loadPathAndWaitForInterstitial(PHISHING_HTML_PATH);
        waitForInterstitialDomToLoad();

        Assert.assertTrue(getVisibilityByIdOnInterstitial("extended-reporting-opt-in"));
    }

    /**
     * @return whether {@code domNodeId} is visible on the interstitial page.
     * @throws Exception if the node cannot be found in the interstitial DOM or unable to evaluate
     * JS.
     */
    private boolean getVisibilityByIdOnInterstitial(String domNodeId) throws Exception {
        final String script =
                "(function isNodeVisible(node) {"
                        + "  if (!node) return 'node not found';"
                        + "  return !node.classList.contains('hidden');"
                        + "})(document.getElementById('"
                        + domNodeId
                        + "'))";

        String value =
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, script);
        if (value.equals("true")) {
            return true;
        } else if (value.equals("false")) {
            return false;
        } else {
            throw new Exception("Node not found");
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingUserOptOutOverridesManifest() throws Throwable {
        AwSafeBrowsingConfigHelper.setSafeBrowsingUserOptIn(false);
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(MALWARE_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingUserOptOutOverridesPerWebView() throws Throwable {
        AwSafeBrowsingConfigHelper.setSafeBrowsingUserOptIn(false);
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setSafeBrowsingEnabled(true);
        loadGreenPage();
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), responseUrl);
        assertTargetPageHasLoaded(MALWARE_PAGE_BACKGROUND_COLOR);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingHardcodedMalwareUrl() throws Throwable {
        loadGreenPage();
        mActivityTestRule.loadUrlAsync(mAwContents, WEB_UI_MALWARE_URL);
        // Wait for the interstitial to actually render.
        mActivityTestRule.waitForVisualStateCallback(mAwContents);
        waitForInterstitialDomToLoad();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingHardcodedPhishingUrl() throws Throwable {
        loadGreenPage();
        mActivityTestRule.loadUrlAsync(mAwContents, WEB_UI_PHISHING_URL);
        // Wait for the interstitial to actually render.
        mActivityTestRule.waitForVisualStateCallback(mAwContents);
        waitForInterstitialDomToLoad();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingHardcodedUrlsIgnoreUserOptOut() throws Throwable {
        AwSafeBrowsingConfigHelper.setSafeBrowsingUserOptIn(false);
        loadGreenPage();
        mActivityTestRule.loadUrlAsync(mAwContents, WEB_UI_MALWARE_URL);
        // Wait for the interstitial to actually render.
        mActivityTestRule.waitForVisualStateCallback(mAwContents);
        waitForInterstitialDomToLoad();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingHardcodedUrlsRespectPerWebviewToggle() throws Throwable {
        mActivityTestRule.getAwSettingsOnUiThread(mAwContents).setSafeBrowsingEnabled(false);
        mActivityTestRule.loadUrlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), WEB_UI_MALWARE_URL);
        // If we get here, it means the navigation was not blocked by an interstitial.
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingClickLearnMoreLink() throws Throwable {
        loadInterstitialAndClickLink(
                PHISHING_HTML_PATH,
                "learn-more-link",
                appendLocale("https://support.google.com/chrome/?p=cpn_safe_browsing_wv"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingClickReportErrorLink() throws Throwable {
        // Only phishing interstitials have the report-error-link
        final String reportErrorUrl =
                Uri.parse("https://safebrowsing.google.com/safebrowsing/report_error/")
                        .buildUpon()
                        .appendQueryParameter(
                                "url", mTestServer.getURL(PHISHING_HTML_PATH).toString())
                        .appendQueryParameter("hl", getSafeBrowsingLocaleOnUiThreadForTesting())
                        .toString();
        loadInterstitialAndClickLink(PHISHING_HTML_PATH, "report-error-link", reportErrorUrl);
    }

    private String appendLocale(String url) throws Exception {
        return Uri.parse(url)
                .buildUpon()
                .appendQueryParameter("hl", getSafeBrowsingLocaleOnUiThreadForTesting())
                .toString();
    }

    private String getSafeBrowsingLocaleOnUiThreadForTesting() throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> AwContents.getSafeBrowsingLocaleForTesting());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingClickDiagnosticLink() throws Throwable {
        final String responseUrl = mTestServer.getURL(MALWARE_HTML_PATH);
        final String diagnosticUrl =
                Uri.parse("https://transparencyreport.google.com/safe-browsing/search")
                        .buildUpon()
                        .appendQueryParameter("url", responseUrl)
                        .appendQueryParameter("hl", getSafeBrowsingLocaleOnUiThreadForTesting())
                        .toString();
        loadInterstitialAndClickLink(MALWARE_HTML_PATH, "diagnostic-link", diagnosticUrl);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingClickWhitePaperLink() throws Throwable {
        final String whitepaperUrl =
                Uri.parse("https://www.google.com/chrome/browser/privacy/whitepaper.html")
                        .buildUpon()
                        .appendQueryParameter("hl", getSafeBrowsingLocaleOnUiThreadForTesting())
                        .fragment("extendedreport")
                        .toString();
        loadInterstitialAndClickLink(PHISHING_HTML_PATH, "whitepaper-link", whitepaperUrl);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeBrowsingClickPrivacyPolicy() throws Throwable {
        final String privacyPolicyUrl =
                Uri.parse("https://www.google.com/chrome/browser/privacy/")
                        .buildUpon()
                        .appendQueryParameter("hl", getSafeBrowsingLocaleOnUiThreadForTesting())
                        .fragment("safe-browsing-policies")
                        .toString();
        loadInterstitialAndClickLink(PHISHING_HTML_PATH, "privacy-link", privacyPolicyUrl);
    }

    private void loadInterstitialAndClickLink(String path, String linkId, String linkUrl)
            throws Exception {
        loadPathAndWaitForInterstitial(path);
        waitForInterstitialDomToLoad();
        int pageFinishedCount = mContentsClient.getOnPageFinishedHelper().getCallCount();
        clickLinkById(linkId);
        mContentsClient.getOnPageFinishedHelper().waitForCallback(pageFinishedCount);
        // Some click tests involve URLs that redirect and mAwContents.getUrl() sometimes
        // returns the post-redirect URL, so we instead check with ShouldInterceptRequest.
        AwContentsClient.AwWebResourceRequest requestsForUrl =
                mContentsClient.getShouldInterceptRequestHelper().getRequestsForUrl(linkUrl);
        // Make sure the URL was seen for a main frame navigation.
        Assert.assertTrue(requestsForUrl.isOutermostMainFrame);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testInitSafeBrowsingCallbackOnUIThread() throws Throwable {
        Context ctx =
                InstrumentationRegistry.getInstrumentation()
                        .getTargetContext()
                        .getApplicationContext();
        CallbackHelper helper = new CallbackHelper();
        int count = helper.getCallCount();
        mOnUiThread = false;
        AwContentsStatics.initSafeBrowsing(
                ctx,
                b -> {
                    mOnUiThread = ThreadUtils.runningOnUiThread();
                    helper.notifyCalled();
                });
        helper.waitForCallback(count);
        // Don't run the assert on the callback's thread, since the test runner loses the stack
        // trace unless on the instrumentation thread.
        Assert.assertTrue("Callback should run on UI Thread", mOnUiThread);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testInitSafeBrowsingUsesAppContext() throws Throwable {
        MockContext ctx =
                new MockContext(InstrumentationRegistry.getInstrumentation().getTargetContext());
        CallbackHelper helper = new CallbackHelper();
        int count = helper.getCallCount();

        AwContentsStatics.initSafeBrowsing(ctx, b -> helper.notifyCalled());
        helper.waitForCallback(count);
        Assert.assertTrue(
                "Should only use application context", ctx.wasGetApplicationContextCalled());
    }

    private static class MockContext extends ContextWrapper {
        private boolean mGetApplicationContextWasCalled;

        public MockContext(Context context) {
            super(context);
        }

        @Override
        public Context getApplicationContext() {
            mGetApplicationContextWasCalled = true;
            return super.getApplicationContext();
        }

        public boolean wasGetApplicationContextCalled() {
            return mGetApplicationContextWasCalled;
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testGetSafeBrowsingPrivacyPolicyUrl() throws Throwable {
        final Uri privacyPolicyUrl =
                Uri.parse("https://www.google.com/chrome/browser/privacy/")
                        .buildUpon()
                        .appendQueryParameter("hl", getSafeBrowsingLocaleOnUiThreadForTesting())
                        .fragment("safe-browsing-policies")
                        .build();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPrivacyPolicyUrl = AwContentsStatics.getSafeBrowsingPrivacyPolicyUrl();
                });
        Assert.assertEquals(privacyPolicyUrl, this.mPrivacyPolicyUrl);
        Assert.assertNotNull(this.mPrivacyPolicyUrl);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testDestroyWebViewWithInterstitialShowing() throws Throwable {
        loadPathAndWaitForInterstitial(MALWARE_HTML_PATH);
        destroyOnMainSync();
        // As long as we've reached this line without crashing, there should be no bug.
    }

    private void destroyOnMainSync() {
        // The AwActivityTestRule method invokes AwContents#destroy() on the main thread, but
        // Awcontents#destroy() posts an asynchronous task itself to destroy natives. Therefore, we
        // still need to wait for the real work to actually finish.
        mActivityTestRule.destroyAwContentsOnMainSync(mAwContents);
        CriteriaHelper.pollUiThread(
                () -> {
                    try {
                        int awContentsCount =
                                ThreadUtils.runOnUiThreadBlocking(
                                        () -> AwContents.getNativeInstanceCount());
                        Criteria.checkThat(awContentsCount, Matchers.is(0));
                    } catch (Exception e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                });
    }
}
