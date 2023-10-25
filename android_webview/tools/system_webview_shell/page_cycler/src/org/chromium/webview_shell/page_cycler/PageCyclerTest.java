// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell.page_cycler;

import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.BaseJUnit4RunnerDelegate;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Restriction;
import org.chromium.webview_shell.PageCyclerTestActivity;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests running on bots with internet connection to load popular urls,
 * making sure webview doesn't crash
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(BaseJUnit4RunnerDelegate.class)
public class PageCyclerTest {
    private static final long TIMEOUT_IN_SECS = 20;

    private static class WebsiteParams implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            return Arrays.asList(
                    new ParameterSet().value("http://google.com").name("Google"),
                    new ParameterSet().value("http://facebook.com").name("Facebook"),
                    new ParameterSet().value("http://wikipedia.org").name("Wikipedia"),
                    new ParameterSet().value("http://amazon.com").name("Amazon"),
                    new ParameterSet().value("http://youtube.com").name("Youtube"),
                    new ParameterSet().value("http://yahoo.com").name("Yahoo"),
                    new ParameterSet().value("http://ebay.com").name("Ebay"),
                    new ParameterSet().value("http://reddit.com").name("reddit"));
        }
    }

    @Rule
    public BaseActivityTestRule<PageCyclerTestActivity> mRule =
            new BaseActivityTestRule<>(PageCyclerTestActivity.class);

    @Before
    public void setUp() {
        mRule.launchActivity(null);
    }

    @Test
    @LargeTest
    @UseMethodParameter(WebsiteParams.class)
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    public void testVisitPage(String url) {
        final PageCyclerWebViewClient pageCyclerWebViewClient = new PageCyclerWebViewClient();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        new Runnable() {
                            @Override
                            public void run() {
                                final WebView view = mRule.getActivity().getWebView();
                                WebSettings settings = view.getSettings();
                                settings.setJavaScriptEnabled(true);
                                view.setWebViewClient(pageCyclerWebViewClient);
                            }
                        });
        CallbackHelper pageFinishedCallback = pageCyclerWebViewClient.getPageFinishedCallback();
        CallbackHelper errorCallback = pageCyclerWebViewClient.getErrorCallback();
        loadUrlSync(url, pageFinishedCallback, errorCallback);
    }

    private static class PageCyclerWebViewClient extends WebViewClient {
        private final CallbackHelper mPageFinishedCallback;
        private final CallbackHelper mErrorCallback;

        public PageCyclerWebViewClient() {
            super();
            mPageFinishedCallback = new CallbackHelper();
            mErrorCallback = new CallbackHelper();
        }

        public CallbackHelper getPageFinishedCallback() {
            return mPageFinishedCallback;
        }

        public CallbackHelper getErrorCallback() {
            return mErrorCallback;
        }

        @Override
        public void onPageFinished(WebView view, String url) {
            mPageFinishedCallback.notifyCalled();
        }

        // TODO(yolandyan@): create helper class to manage network error
        @Override
        public void onReceivedError(
                WebView webview, int code, String description, String failingUrl) {
            mErrorCallback.notifyCalled();
        }
    }

    private void loadUrlSync(
            final String url,
            final CallbackHelper pageFinishedCallback,
            final CallbackHelper errorCallback) {
        boolean timeout = false;
        int pageFinishedCount = pageFinishedCallback.getCallCount();
        int errorCount = errorCallback.getCallCount();
        loadUrlAsync(url);
        try {
            pageFinishedCallback.waitForCallback(
                    pageFinishedCount, pageFinishedCount + 1, TIMEOUT_IN_SECS, TimeUnit.SECONDS);
        } catch (TimeoutException ex) {
            timeout = true;
        }
        Assert.assertEquals(
                String.format("Network error while accessing %s", url),
                errorCount,
                errorCallback.getCallCount());
        Assert.assertFalse(String.format("Timeout error while accessing %s", url), timeout);
    }

    private void loadUrlAsync(final String url) {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        new Runnable() {
                            @Override
                            public void run() {
                                WebView view = mRule.getActivity().getWebView();
                                view.loadUrl(url);
                            }
                        });
    }
}
