// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.test.sensitive_content;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;

import android.os.Build;
import android.view.View;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.AwActivityTestRule;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.AwTestContainerView;
import org.chromium.android_webview.test.TestAwContentsClient;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.sensitive_content.SensitiveContentFeatures;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.ViewAndroidDelegate;

/** Tests that the content sensitivity is set properly on WebView. */
@RunWith(AwJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures(SensitiveContentFeatures.SENSITIVE_CONTENT)
@MinAndroidSdkLevel(Build.VERSION_CODES.VANILLA_ICE_CREAM)
public class AwSensitiveContentTest {
    public static final String SENSITIVE_FILE =
            "/android_webview/test/data/autofill/page_address_credit_card_forms.html";
    public static final String NOT_SENSITIVE_FILE =
            "/android_webview/test/data/autofill/form_with_datalist.html";

    @Rule public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    private EmbeddedTestServer mTestServer;
    private TestAwContentsClient mTestAwContentsClient;
    private AwTestContainerView mTestContainerView;
    private AwContents mAwContents;

    @Before
    public void setUp() throws Exception {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        InstrumentationRegistry.getInstrumentation().getContext());
        mTestAwContentsClient = new TestAwContentsClient();
        mTestContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mTestAwContentsClient);
        mAwContents = mTestContainerView.getAwContents();
    }

    @After
    public void tearDown() {
        if (mTestServer != null) {
            mTestServer.stopAndDestroyServer();
        }
    }

    @Test
    @MediumTest
    public void testWebViewHasSensitiveContentWhileSensitiveFieldsArePresent() throws Exception {
        Assert.assertEquals(
                "Initially, the page does not have sensitive content",
                View.CONTENT_SENSITIVITY_AUTO,
                mTestContainerView.getContentSensitivity());

        mActivityTestRule.loadUrlAsync(mAwContents, mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(
                () ->
                        mTestContainerView.getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_SENSITIVE);

        mActivityTestRule.loadUrlAsync(mAwContents, mTestServer.getURL(NOT_SENSITIVE_FILE));
        pollUiThread(
                () ->
                        mTestContainerView.getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_NOT_SENSITIVE);
    }

    @Test
    @MediumTest
    public void testSwapViewAndroidDelegate() {
        mActivityTestRule.loadUrlAsync(mAwContents, mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(
                () ->
                        mTestContainerView.getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_SENSITIVE);

        WebContents webContents = mAwContents.getWebContents();
        ContentView[] newContainerViewHolder = new ContentView[1];
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ContentView newContainerView =
                            ContentView.createContentView(
                                    mActivityTestRule.getActivity(), webContents);
                    newContainerViewHolder[0] = newContainerView;
                    ViewAndroidDelegate newViewAndroidDelegate =
                            ViewAndroidDelegate.createBasicDelegate(newContainerView);
                    Assert.assertEquals(
                            "Initially, the content view does not have sensitive content",
                            View.CONTENT_SENSITIVITY_AUTO,
                            newContainerView.getContentSensitivity());

                    webContents.setDelegates(
                            "",
                            newViewAndroidDelegate,
                            newContainerView,
                            null,
                            WebContents.createDefaultInternalsHolder());
                });

        pollUiThread(
                () ->
                        newContainerViewHolder[0].getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_SENSITIVE);
    }
}
