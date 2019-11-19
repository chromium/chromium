// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.junit.Assert.assertEquals;

import android.support.test.filters.SmallTest;
import android.view.KeyEvent;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;

import org.chromium.android_webview.AwContents;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MetricsUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;

import java.util.concurrent.Callable;

/**
 * AwAutocompleteTest only runs below Android O.
 */
@DisabledTest
public class AwAutocompleteTest {
    public static final String FILE = "/login.html";
    public static final String TITLE = "DONE";

    @Rule
    public AwActivityTestRule mRule = new AwActivityTestRule();
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;
    private TestAwContentsClient mContentsClient;
    private TestWebServer mWebServer;
    private MetricsUtils.HistogramDelta mAutocompleteEnabled;
    private MetricsUtils.HistogramDelta mAutocompleteDisabled;

    private void loadUrlSync(String url) throws Exception {
        mRule.loadUrlSync(
                mTestContainerView.getAwContents(), mContentsClient.getOnPageFinishedHelper(), url);
    }

    private void initUmaDeltaSamples() {
        mAutocompleteEnabled =
                new MetricsUtils.HistogramDelta("Autofill.AutocompleteEnabled", 1 /*true*/);
        mAutocompleteDisabled =
                new MetricsUtils.HistogramDelta("Autofill.AutocompleteEnabled", 0 /*false*/);
    }

    private void verifyUmaAutocompleteEnabled(final boolean enabled) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            if (enabled) {
                assertEquals(1, mAutocompleteEnabled.getDelta());
                assertEquals(0, mAutocompleteDisabled.getDelta());
            } else {
                assertEquals(0, mAutocompleteEnabled.getDelta());
                assertEquals(1, mAutocompleteDisabled.getDelta());
            }
        });
    }

    private void verifyUmaNotRecorded() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertEquals(0, mAutocompleteEnabled.getDelta());
            assertEquals(0, mAutocompleteDisabled.getDelta());
        });
    }

    private String executeJavaScriptAndWaitForResult(String code) throws Throwable {
        return mRule.executeJavaScriptAndWaitForResult(
                mTestContainerView.getAwContents(), mContentsClient, code);
    }

    private void dispatchDownAndUpKeyEvents(final int code) throws Throwable {
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, code));
        dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, code));
    }

    private boolean dispatchKeyEvent(final KeyEvent event) throws Throwable {
        return TestThreadUtils.runOnUiThreadBlocking(new Callable<Boolean>() {
            @Override
            public Boolean call() {
                return mTestContainerView.dispatchKeyEvent(event);
            }
        });
    }

    private void waitAutocompleteDone() throws Throwable {
        // There is no available event to let us know that the autocomplete has done, to make test
        // simple, just change the the title by javascript and wait for title being changed. This
        // worked most of time. If test become flaky, this parts should be investigated first.
        executeJavaScriptAndWaitForResult("document.title='" + TITLE + "';");
        mRule.pollUiThread(() -> TITLE.equals(mAwContents.getTitle()));
    }

    private void loadPage() throws Throwable {
        final String data = "<html><head></head><body><form action='a.html' name='formname'>"
                + "<input type='text' id='text1' name='username'"
                + " placeholder='placeholder@placeholder.com' autocomplete='username name'>"
                + "<input type='submit'>"
                + "</form></body></html>";
        final String url = mWebServer.setResponse(FILE, data, null);
        loadUrlSync(url);
    }

    private void triggerAutocomplete() throws Throwable {
        executeJavaScriptAndWaitForResult("document.getElementById('text1').select();");
        dispatchDownAndUpKeyEvents(KeyEvent.KEYCODE_A);
    }

    private void disableAwAutocomplete() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mAwContents.getSettings().setSaveFormData(false));
    }

    @Before
    public void setUp() throws Exception {
        initUmaDeltaSamples();
        mContentsClient = new TestAwContentsClient();
        mTestContainerView = mRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mTestContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        mWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() {
        mWebServer.shutdown();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAutocompleteEnabledUMA() throws Throwable {
        loadPage();
        // Verify that Uma isn't recorded before autocomplete is triggered.
        verifyUmaNotRecorded();
        triggerAutocomplete();
        waitAutocompleteDone();
        verifyUmaAutocompleteEnabled(true);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testAutocompleteDisabledUMA() throws Throwable {
        disableAwAutocomplete();
        loadPage();
        // Verify that Uma isn't recorded before autocomplete is triggered.
        verifyUmaNotRecorded();
        triggerAutocomplete();
        waitAutocompleteDone();
        verifyUmaAutocompleteEnabled(false);
    }
}
