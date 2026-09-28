// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Bundle;

import androidx.test.InstrumentationRegistry;
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
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.content_public.browser.test.util.HistoryUtils;
import org.chromium.net.test.util.TestWebServer;

/**
 * Tests for the {@link android.webkit.WebView#saveState} and {@link
 * android.webkit.WebView#restoreState} APIs.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class SaveRestoreStateTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private static class TestVars {
        public final TestAwContentsClient contentsClient;
        public final AwTestContainerView testView;
        public final AwContents awContents;
        public final NavigationController navigationController;

        public TestVars(TestAwContentsClient contentsClient, AwTestContainerView testView) {
            this.contentsClient = contentsClient;
            this.testView = testView;
            this.awContents = testView.getAwContents();
            this.navigationController = this.awContents.getNavigationController();
        }
    }

    private TestVars createNewView() {
        TestAwContentsClient contentsClient = new TestAwContentsClient();
        AwTestContainerView testView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        return new TestVars(contentsClient, testView);
    }

    private TestVars mVars;
    private TestWebServer mWebServer;

    private static final int NUM_NAVIGATIONS = 3;
    private static final String[] TITLES = {
        "page 1 title foo", "page 2 title bar", "page 3 title baz"
    };
    private static final String[] PATHS = {
        "/p1foo.html", "/p2bar.html", "/p3baz.html",
    };

    private String[] mUrls;

    public SaveRestoreStateTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        mVars = createNewView();
        mUrls = new String[NUM_NAVIGATIONS];
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() {
        if (mWebServer != null) {
            mWebServer.shutdown();
        }
    }

    private void setServerResponseAndLoad(TestVars vars, int upto) throws Throwable {
        for (int i = 0; i < upto; ++i) {
            String html = CommonResources.makeHtmlPageFrom("<title>" + TITLES[i] + "</title>", "");
            mUrls[i] = mWebServer.setResponse(PATHS[i], html, null);

            mActivityTestRule.loadUrlSync(
                    vars.awContents, vars.contentsClient.getOnPageFinishedHelper(), mUrls[i]);
        }
    }

    private NavigationHistory getNavigationHistoryOnUiThread(final TestVars vars) throws Throwable {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> vars.navigationController.getNavigationHistory());
    }

    private void checkHistoryItemList(TestVars vars) throws Throwable {
        NavigationHistory history = getNavigationHistoryOnUiThread(vars);
        Assert.assertEquals(NUM_NAVIGATIONS, history.getEntryCount());
        Assert.assertEquals(NUM_NAVIGATIONS - 1, history.getCurrentEntryIndex());

        // Note this is not meant to be a thorough test of NavigationHistory,
        // but is only meant to test enough to make sure state is restored.
        // See NavigationHistoryTest for more thorough tests.
        for (int i = 0; i < NUM_NAVIGATIONS; ++i) {
            Assert.assertEquals(mUrls[i], history.getEntryAtIndex(i).getOriginalUrl().getSpec());
            Assert.assertEquals(mUrls[i], history.getEntryAtIndex(i).getUrl().getSpec());
            Assert.assertEquals(TITLES[i], history.getEntryAtIndex(i).getTitle());
        }
    }

    private TestVars saveAndRestoreStateOnUiThread(final TestVars vars) {
        final TestVars restoredVars = createNewView();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            Bundle bundle = new Bundle();
                            boolean result = vars.awContents.saveState(bundle);
                            Assert.assertTrue(result);
                            result = restoredVars.awContents.restoreState(bundle);
                            Assert.assertTrue(result);
                        });
        return restoredVars;
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSaveRestoreStateWithTitle() throws Throwable {
        setServerResponseAndLoad(mVars, 1);
        final TestVars restoredVars = saveAndRestoreStateOnUiThread(mVars);
        mActivityTestRule.pollUiThread(
                () ->
                        TITLES[0].equals(restoredVars.awContents.getTitle())
                                && TITLES[0].equals(restoredVars.contentsClient.getUpdatedTitle()));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSaveRestoreStateWithHistoryItemList() throws Throwable {
        setServerResponseAndLoad(mVars, NUM_NAVIGATIONS);
        TestVars restoredVars = saveAndRestoreStateOnUiThread(mVars);
        checkHistoryItemList(restoredVars);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSaveRestoreStateWithScrollPosition() throws Throwable {
        AwActivityTestRule.enableJavaScriptOnUiThread(mVars.awContents);
        String headers =
                "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />"
                        + "<style>body { height: 10000px; margin: 0; }</style>";
        String html1 = CommonResources.makeHtmlPageFrom(headers, "<h1>Page 1</h1>");
        String html2 = CommonResources.makeHtmlPageFrom(headers, "<h1>Page 2</h1>");
        String url1 = mWebServer.setResponse(PATHS[0], html1, null);
        String url2 = mWebServer.setResponse(PATHS[1], html2, null);

        mActivityTestRule.loadUrlSync(
                mVars.awContents, mVars.contentsClient.getOnPageFinishedHelper(), url1);

        final double targetScrollY = 500.0;
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mVars.awContents,
                mVars.contentsClient,
                "window.scrollTo(0, " + targetScrollY + ");");
        AwActivityTestRule.pollInstrumentationThread(
                () -> {
                    double scrollY =
                            Double.parseDouble(
                                    mActivityTestRule.executeJavaScriptAndWaitForResult(
                                            mVars.awContents,
                                            mVars.contentsClient,
                                            "window.scrollY"));
                    return Math.abs(targetScrollY - scrollY) < 1.0;
                });

        // Navigate to another page so that the first page's scroll position is saved to its
        // navigation entry.
        mActivityTestRule.loadUrlSync(
                mVars.awContents, mVars.contentsClient.getOnPageFinishedHelper(), url2);

        // Save and restore state into a new view, waiting for the restored current entry to load.
        TestVars restoredVars = saveAndRestoreStateOnUiThread(mVars);
        AwActivityTestRule.enableJavaScriptOnUiThread(restoredVars.awContents);
        restoredVars.contentsClient.getOnPageFinishedHelper().waitForCallback(0);

        // Navigate back to the first page.
        HistoryUtils.goBackSync(
                InstrumentationRegistry.getInstrumentation(),
                restoredVars.awContents.getWebContents(),
                restoredVars.contentsClient.getOnPageFinishedHelper());

        // Verify that the scroll position is preserved.
        AwActivityTestRule.pollInstrumentationThread(
                () -> {
                    double restoredScroll =
                            Double.parseDouble(
                                    mActivityTestRule.executeJavaScriptAndWaitForResult(
                                            restoredVars.awContents,
                                            restoredVars.contentsClient,
                                            "window.scrollY"));
                    return Math.abs(targetScrollY - restoredScroll) < 1.0;
                });
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testRestoreFromInvalidStateFails() throws Throwable {
        final Bundle invalidState = new Bundle();
        invalidState.putByteArray(AwContents.SAVE_RESTORE_STATE_KEY, "invalid state".getBytes());
        boolean result =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> mVars.awContents.restoreState(invalidState));
        Assert.assertFalse(result);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSaveStateForNoNavigationFails() throws Throwable {
        final Bundle state = new Bundle();
        boolean result =
                ThreadUtils.runOnUiThreadBlocking(() -> mVars.awContents.restoreState(state));
        Assert.assertFalse(result);
    }
}
