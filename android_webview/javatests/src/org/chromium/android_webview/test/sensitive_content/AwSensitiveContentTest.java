// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.test.sensitive_content;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.base.test.util.CriteriaHelper.pollUiThreadNested;

import android.os.Build;
import android.view.View;

import androidx.test.InstrumentationRegistry;
import androidx.test.annotation.UiThreadTest;
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
                "Initially, the page does not have sensitive content",
                View.CONTENT_SENSITIVITY_AUTO,
                mTestContainerView.getContentSensitivity());

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

    @Test
    @MediumTest
    @UiThreadTest
    public void testSwapViewAndroidDelegate() {
        sActivityTestRule.loadUrlAsync(mAwContents, mTestServer.getURL(SENSITIVE_FILE));
        pollUiThreadNested(
                () ->
                        mTestContainerView.getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_SENSITIVE);

        WebContents webContents = mAwContents.getWebContents();
        ContentView newContainerView =
                ContentView.createContentView(sActivityTestRule.getActivity(), webContents);
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
        pollUiThreadNested(
                () ->
                        newContainerView.getContentSensitivity()
                                == View.CONTENT_SENSITIVITY_SENSITIVE);
    }
}
