// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.app.Activity;
import android.os.Build;
import android.view.View;
import android.view.WindowManager;

import androidx.annotation.RequiresApi;
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
import org.chromium.android_webview.AwDisplayCutoutController.Insets;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.net.test.util.TestWebServer;

/** Tests for DisplayCutout. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@MinAndroidSdkLevel(Build.VERSION_CODES.P)
@CommandLineFlags.Add({"enable-features=" + AwFeatures.WEBVIEW_DISPLAY_CUTOUT})
@RequiresApi(Build.VERSION_CODES.P)
public class AwDisplayCutoutTest extends AwParameterizedTest {
    private static final String TEST_HTML =
            "<html><head><style>\n"
                    + "body {\n"
                    + " margin: 0;\n"
                    + " padding: 0pt 0pt 0pt 0pt;\n"
                    + "}\n"
                    + "div {\n"
                    + " margin: 0;\n"
                    + " padding: env(safe-area-inset-top) "
                    + "          env(safe-area-inset-right)"
                    + "          env(safe-area-inset-bottom)"
                    + "          env(safe-area-inset-left);\n"
                    + "}\n"
                    + "</style></head><body>\n"
                    + "<div id='text'>"
                    + "On notched phones, there should be enough padding on the top"
                    + " to not have this text appear under the statusbar/notch.\n"
                    + "</div>\n"
                    + "</body></html>";

    @Rule public AwActivityTestRule mActivityTestRule;

    public AwDisplayCutoutTest(AwSettingsMutation param) {
        mActivityTestRule =
                new AwActivityTestRule(param.getMutation()) {
                    @Override
                    public boolean needsHideActionBar() {
                        // If action bar is showing, WebView cannot be fully occupying the screen.
                        return true;
                    }
                };
    }

    private TestWebServer mWebServer;
    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mContainerView;
    private AwContents mAwContents;

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();

        mContentsClient = new TestAwContentsClient();
        mContainerView = mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = mContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        // In pre-R, we need to explicitly set this to draw under notch.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Activity activity = mActivityTestRule.getActivity();
                    activity.getWindow().getAttributes().layoutInDisplayCutoutMode =
                            WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
                });
    }

    @After
    public void tearDown() {
        mWebServer.shutdown();
    }

    private void setFullscreen(boolean fullscreen) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Activity activity = mActivityTestRule.getActivity();
                    View decor = activity.getWindow().getDecorView();

                    int systemUiVisibility = decor.getSystemUiVisibility();
                    int flags =
                            View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                                    | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                                    | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                                    | View.SYSTEM_UI_FLAG_FULLSCREEN
                                    | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
                    if (fullscreen) {
                        activity.getWindow()
                                .setFlags(
                                        WindowManager.LayoutParams.FLAG_FULLSCREEN,
                                        WindowManager.LayoutParams.FLAG_FULLSCREEN);
                        systemUiVisibility |= flags;
                    } else {
                        activity.getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
                        systemUiVisibility &= flags;
                    }
                    decor.setSystemUiVisibility(systemUiVisibility);
                });
    }

    @Test
    @SmallTest
    public void testNoSafeAreaSet() throws Throwable {
        setFullscreen(true);
        mActivityTestRule.loadHtmlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), TEST_HTML);
        // Reset safe area just in case we have a notch.
        Insets insets = new Insets(0, 0, 0, 0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.getDisplayCutoutController().onApplyWindowInsetsInternal(insets);
                });
        final String code =
                "window.getComputedStyle(document.getElementById('text'))"
                        + ".getPropertyValue('padding-top')";
        Assert.assertEquals(
                "\"0px\"",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, code));
    }

    @Test
    @SmallTest
    public void testSafeAreaSet() throws Throwable {
        setFullscreen(true);
        mActivityTestRule.loadHtmlSync(
                mAwContents, mContentsClient.getOnPageFinishedHelper(), TEST_HTML);
        Insets insets = new Insets(0, 130, 0, 0);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mAwContents.getDisplayCutoutController().onApplyWindowInsetsInternal(insets);
                });
        final String code =
                "window.getComputedStyle(document.getElementById('text'))"
                        + ".getPropertyValue('padding-top')";
        Assert.assertNotEquals(
                "\"0px\"",
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, code));
    }
}
