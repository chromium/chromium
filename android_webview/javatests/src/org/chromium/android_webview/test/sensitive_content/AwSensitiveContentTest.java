// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.test.sensitive_content;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;

import android.os.Build;
import android.view.View;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.test.AwActivityTestRule;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.AwTestContainerView;
import org.chromium.android_webview.test.TestAwContentsClient;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.components.sensitive_content.SensitiveContentFeatures;
import org.chromium.net.test.EmbeddedTestServer;

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

    @ClassRule public static AwActivityTestRule sActivityTestRule = new AwActivityTestRule();

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
                sActivityTestRule.createAwTestContainerViewOnMainSync(mTestAwContentsClient);
        mAwContents = mTestContainerView.getAwContents();
    }

    @Test
    @MediumTest
    public void testWebViewHasSensitiveContentWhileSensitiveFieldsArePresent() throws Exception {
        Assert.assertEquals(
                "Initially, the does not have sensitive content",
                mTestContainerView.getContentSensitivity(),
                View.CONTENT_SENSITIVITY_AUTO);

        sActivityTestRule.loadUrlAsync(mAwContents, mTestServer.getURL(SENSITIVE_FILE));
        pollUiThread(
                () ->
                        mTestContainerView.getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_SENSITIVE);

        sActivityTestRule.loadUrlAsync(mAwContents, mTestServer.getURL(NOT_SENSITIVE_FILE));
        pollUiThread(
                () ->
                        mTestContainerView.getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_NOT_SENSITIVE);
    }
}
