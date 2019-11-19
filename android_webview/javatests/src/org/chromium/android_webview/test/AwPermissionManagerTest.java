// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.Handler;
import android.os.Looper;
import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.permission.AwPermissionRequest;
import org.chromium.android_webview.test.util.CommonResources;
import org.chromium.base.test.util.Feature;
import org.chromium.net.test.util.TestWebServer;

/**
 * Test AwPermissionManager.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AwPermissionManagerTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private static final String REQUEST_DUPLICATE = "<html> <script>"
            + "navigator.requestMIDIAccess({sysex: true}).then(function() {"
            + "});"
            + "navigator.requestMIDIAccess({sysex: true}).then(function() {"
            + "  window.document.title = 'second-granted';"
            + "});"
            + "</script><body>"
            + "</body></html>";

    private TestWebServer mTestWebServer;
    private String mPage;

    @Before
    public void setUp() throws Exception {
        mTestWebServer = TestWebServer.start();
    }

    @After
    public void tearDown() {
        mTestWebServer.shutdown();
        mTestWebServer = null;
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testRequestMultiple() {
        mPage = mTestWebServer.setResponse("/permissions", REQUEST_DUPLICATE,
                CommonResources.getTextHtmlHeaders(true));

        TestAwContentsClient contentsClient = new TestAwContentsClient() {
            private boolean mCalled;

            @Override
            public void onPermissionRequest(final AwPermissionRequest awPermissionRequest) {
                if (mCalled) {
                    Assert.fail("Only one request was expected");
                    return;
                }
                mCalled = true;

                // Emulate a delayed response to the request by running four seconds in the future.
                Handler handler = new Handler(Looper.myLooper());
                handler.postDelayed(() -> awPermissionRequest.grant(), 4000);
            }
        };

        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(contentsClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(awContents);
        mActivityTestRule.loadUrlAsync(awContents, mPage, null);
        pollTitleAs("second-granted", awContents);
    }

    private void pollTitleAs(final String title, final AwContents awContents) {
        AwActivityTestRule.pollInstrumentationThread(
                () -> title.equals(mActivityTestRule.getTitleOnUiThread(awContents)));
    }
}

