// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static androidx.test.espresso.intent.Intents.intending;

import static org.hamcrest.Matchers.not;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Intent;
import android.net.Uri;
import android.os.ParcelFileDescriptor;

import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Executor;

/** Test integration with content restriction on WebViews. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@Batch(Batch.PER_CLASS)
public class AwContentRestrictionTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private static final String ALLOWED_SITE_1_TITLE = "Allowed Site 1";
    private static final String ALLOWED_SITE_1_PATH = "/allowed1.html";
    private static final String ALLOWED_SITE_2_TITLE = "Allowed Site 2";
    private static final String ALLOWED_SITE_2_PATH = "/allowed2.html";
    private static final String BLOCKED_SITE_TITLE = "Blocked Site";
    private static final String BLOCKED_SITE_PATH = "/blocked.html";
    private static final String GO_BACK_LINK_ID = "back-link";
    private static final String LEARN_MORE_LINK_ID = "learn-more-link";

    private AwContents mAwContents;
    private TestWebServer mWebServer;
    private final TestAwContentsClient mContentsClient = new TestAwContentsClient();

    /**
     * Test implementation of {@link AconfigFlaggedApiDelegate} to mock out Android platform
     * dependencies for the content restriction feature.
     */
    private class TestAconfigDelegate implements AconfigFlaggedApiDelegate {
        @Override
        public boolean isContentRestrictionEnabled() {
            return true;
        }

        @Override
        public Promise<Boolean> requestContentRestrictionClassification(
                Uri uri, ParcelFileDescriptor requestBody, String mimeType, Executor executor) {
            boolean allow = true;
            if (uri.getPath().contains(BLOCKED_SITE_PATH)) {
                allow = false;
            }
            return Promise.fulfilled(allow);
        }

        @Override
        public boolean sendShowRestrictedContentIntent(Uri uri) {
            Intent intent = new Intent(Intent.ACTION_VIEW, uri);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            mActivityTestRule.getActivity().startActivity(intent);
            return true;
        }
    }

    public AwContentRestrictionTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();
        AconfigFlaggedApiDelegate.setInstanceForTesting(new TestAconfigDelegate());

        AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        setUpWebServer();
    }

    @After
    public void tearDown() throws Exception {
        AconfigFlaggedApiDelegate.setInstanceForTesting(null);
        mActivityTestRule.destroyAwContentsOnMainSync(mAwContents);
        mWebServer.shutdown();
    }

    private void setUpWebServer() {
        mWebServer.setResponse(
                ALLOWED_SITE_1_PATH,
                "<html><head><title>" + ALLOWED_SITE_1_TITLE + "</title></head></html>",
                null);
        mWebServer.setResponse(
                ALLOWED_SITE_2_PATH,
                "<html><head><title>" + ALLOWED_SITE_2_TITLE + "</title></head></html>",
                null);
        mWebServer.setResponse(
                BLOCKED_SITE_PATH,
                "<html><head><title>" + BLOCKED_SITE_TITLE + "</title></head></html>",
                null);
    }

    private void clickLinkById(String id) throws Exception {
        final String script = "document.getElementById('" + id + "').click()";
        mActivityTestRule.executeJavaScriptAndWaitForResult(mAwContents, mContentsClient, script);
    }

    private void waitForInterstitialPageLoad() {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        String result =
                                mActivityTestRule.executeJavaScriptAndWaitForResult(
                                        mAwContents,
                                        mContentsClient,
                                        "document.getElementById('"
                                                + LEARN_MORE_LINK_ID
                                                + "') != null",
                                        false);
                        return result.equals("true");
                    } catch (Exception e) {
                        return false;
                    }
                },
                "Interstitial element should appear to confirm page load",
                AwActivityTestRule.WAIT_TIMEOUT_MS,
                AwActivityTestRule.CHECK_INTERVAL);
    }

    private int getNavigationHistoryEntryCount() throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.getNavigationHistory().getEntryCount());
    }

    private int getNavigationHistoryCurrentEntryIndex() throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.getNavigationHistory().getCurrentEntryIndex());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testAllowedSites() throws Throwable {
        int initialHistoryCount = getNavigationHistoryEntryCount();
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                mWebServer.getResponseUrl(ALLOWED_SITE_1_PATH));

        // The first load replaces the initial empty entry, so the count does not increase.
        Assert.assertEquals(initialHistoryCount, getNavigationHistoryEntryCount());
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                mWebServer.getResponseUrl(ALLOWED_SITE_2_PATH));

        // The second load adds a new entry, so the count increases by 1.
        Assert.assertEquals(initialHistoryCount + 1, getNavigationHistoryEntryCount());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testClickLearnMoreDispatchesIntent() throws Throwable {
        mActivityTestRule.loadUrlAsync(mAwContents, mWebServer.getResponseUrl(BLOCKED_SITE_PATH));
        waitForInterstitialPageLoad();
        try {
            Intents.init();
            intending(not(IntentMatchers.isInternal()))
                    .respondWith(new ActivityResult(Activity.RESULT_OK, null));
            clickLinkById(LEARN_MORE_LINK_ID);
            CriteriaHelper.pollInstrumentationThread(
                    () -> Intents.getIntents().size() > 0, "An intent should be received");

            Intent intent = Intents.getIntents().get(0);
            Assert.assertEquals(Intent.ACTION_VIEW, intent.getAction());
            Assert.assertEquals(
                    mWebServer.getResponseUrl(BLOCKED_SITE_PATH), intent.getDataString());
        } finally {
            Intents.release();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @EnableFeatures({AwFeatures.WEBVIEW_CONTENT_RESTRICTION_SUPPORT})
    public void testClickGoBackTriggersNavigation() throws Throwable {
        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                mWebServer.getResponseUrl(ALLOWED_SITE_1_PATH));
        int initialHistoryCount = getNavigationHistoryEntryCount();
        int initialHistoryIndex = getNavigationHistoryCurrentEntryIndex();

        mActivityTestRule.loadUrlAsync(mAwContents, mWebServer.getResponseUrl(BLOCKED_SITE_PATH));
        waitForInterstitialPageLoad();
        int afterBlockedHistoryCount = getNavigationHistoryEntryCount();
        Assert.assertEquals(
                "Blocked navigation should add a history entry",
                initialHistoryCount + 1,
                afterBlockedHistoryCount);
        clickLinkById(GO_BACK_LINK_ID);

        // Wait for the current history index to point back to the initial index.
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        int currentIndex = getNavigationHistoryCurrentEntryIndex();
                        return currentIndex == initialHistoryIndex;
                    } catch (Exception e) {
                        return false;
                    }
                },
                "History index should return to initial after clicking go back",
                AwActivityTestRule.WAIT_TIMEOUT_MS,
                AwActivityTestRule.CHECK_INTERVAL);
    }
}
