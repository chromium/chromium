// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwSettings;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.display.DisplayAndroid;

import java.util.Locale;

/** Tests for legacy quirks (compatibility with WebView Classic). */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwLegacyQuirksTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    public AwLegacyQuirksTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    // WebView layout width tests are flaky: http://crbug.com/746264
    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @DisabledTest(message = "crbug.com/746264")
    public void testTargetDensityDpi() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSyncInQuirksMode(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        CallbackHelper onPageFinishedHelper = contentClient.getOnPageFinishedHelper();

        final String pageTemplate =
                "<html><head><meta name='viewport' content='width=device-width,"
                        + " target-densityDpi=%s' /></head><body"
                        + " onload='document.title=document.body.clientWidth'></body></html>";
        final String pageDeviceDpi = String.format((Locale) null, pageTemplate, "device-dpi");
        final String pageHighDpi = String.format((Locale) null, pageTemplate, "high-dpi");
        final String pageDpi100 = String.format((Locale) null, pageTemplate, "100");

        settings.setJavaScriptEnabled(true);

        DisplayAndroid displayAndroid =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return DisplayAndroid.getNonMultiDisplay(
                                    InstrumentationRegistry.getInstrumentation()
                                            .getTargetContext());
                        });
        mActivityTestRule.loadDataSync(
                awContents, onPageFinishedHelper, pageDeviceDpi, "text/html", false);
        int actualWidth = Integer.parseInt(mActivityTestRule.getTitleOnUiThread(awContents));
        Assert.assertEquals(displayAndroid.getDisplayWidth(), actualWidth, 10f);

        float displayWidth = displayAndroid.getDisplayWidth();
        float deviceDpi = 160f * displayAndroid.getDipScale();

        mActivityTestRule.loadDataSync(
                awContents, onPageFinishedHelper, pageHighDpi, "text/html", false);
        actualWidth = Integer.parseInt(mActivityTestRule.getTitleOnUiThread(awContents));
        Assert.assertEquals(displayWidth * (240f / deviceDpi), actualWidth, 10f);

        mActivityTestRule.loadDataSync(
                awContents, onPageFinishedHelper, pageDpi100, "text/html", false);
        actualWidth = Integer.parseInt(mActivityTestRule.getTitleOnUiThread(awContents));
        Assert.assertEquals(displayWidth * (100f / deviceDpi), actualWidth, 10f);
    }

    // WebView layout width tests are flaky: http://crbug.com/746264
    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @DisabledTest(message = "crbug.com/746264")
    public void testWideViewportInitialScaleDoesNotExpandFixedLayoutWidth() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSyncInQuirksMode(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        CallbackHelper onPageFinishedHelper = contentClient.getOnPageFinishedHelper();

        final String page =
                "<html><head><meta name='viewport' content='width=device-width, initial-scale=0.5'"
                        + " /></head><body"
                        + " onload='document.title=document.body.clientWidth'></body></html>";

        settings.setJavaScriptEnabled(true);
        settings.setUseWideViewPort(true);

        DisplayAndroid displayAndroid =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return DisplayAndroid.getNonMultiDisplay(
                                    InstrumentationRegistry.getInstrumentation()
                                            .getTargetContext());
                        });
        mActivityTestRule.loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        float displayWidth = displayAndroid.getDisplayWidth() / displayAndroid.getDipScale();
        int actualWidth = Integer.parseInt(mActivityTestRule.getTitleOnUiThread(awContents));
        Assert.assertEquals(displayWidth, actualWidth, 10f);
        Assert.assertEquals(1.0f, mActivityTestRule.getScaleOnUiThread(awContents), 0);
    }

    // WebView layout width tests are flaky: http://crbug.com/746264
    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @DisabledTest(message = "crbug.com/746264")
    public void testZeroValuesQuirk() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSyncInQuirksMode(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        CallbackHelper onPageFinishedHelper = contentClient.getOnPageFinishedHelper();

        final String page =
                "<html><head><meta name='viewport' content='width=0, height=0, initial-scale=0.0,  "
                        + "   minimum-scale=0.0, maximum-scale=0.0' /></head><body"
                        + " onload='document.title=document.body.clientWidth'></body></html>";

        settings.setJavaScriptEnabled(true);

        DisplayAndroid displayAndroid =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return DisplayAndroid.getNonMultiDisplay(
                                    InstrumentationRegistry.getInstrumentation()
                                            .getTargetContext());
                        });
        mActivityTestRule.loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        float displayWidth = displayAndroid.getDisplayWidth() / displayAndroid.getDipScale();
        int actualWidth = Integer.parseInt(mActivityTestRule.getTitleOnUiThread(awContents));
        Assert.assertEquals(displayWidth, actualWidth, 10f);
        Assert.assertEquals(1.0f, mActivityTestRule.getScaleOnUiThread(awContents), 0);

        settings.setUseWideViewPort(true);
        mActivityTestRule.loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        actualWidth = Integer.parseInt(mActivityTestRule.getTitleOnUiThread(awContents));
        Assert.assertEquals(displayWidth, actualWidth, 10f);
        Assert.assertEquals(1.0f, mActivityTestRule.getScaleOnUiThread(awContents), 0);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    @SkipMutations(reason = "This test depends on AwSettings.setUseWideViewPort(false)")
    public void testScreenSizeInPhysicalPixelsQuirk() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSyncInQuirksMode(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        CallbackHelper onPageFinishedHelper = contentClient.getOnPageFinishedHelper();

        settings.setJavaScriptEnabled(true);

        mActivityTestRule.loadUrlSync(
                awContents, onPageFinishedHelper, ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);

        DisplayAndroid displayAndroid =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return DisplayAndroid.getNonMultiDisplay(
                                    InstrumentationRegistry.getInstrumentation()
                                            .getTargetContext());
                        });
        float dipScale = displayAndroid.getDipScale();
        float physicalDisplayWidth = displayAndroid.getDisplayWidth();
        float physicalDisplayHeight = displayAndroid.getDisplayHeight();

        float screenWidth =
                Integer.parseInt(
                        mActivityTestRule.executeJavaScriptAndWaitForResult(
                                awContents, contentClient, "screen.width"));
        Assert.assertEquals(physicalDisplayWidth, screenWidth, 10f);
        float screenAvailWidth =
                Integer.parseInt(
                        mActivityTestRule.executeJavaScriptAndWaitForResult(
                                awContents, contentClient, "screen.availWidth"));
        Assert.assertEquals(physicalDisplayWidth, screenAvailWidth, 10f);
        float outerWidth =
                Integer.parseInt(
                        mActivityTestRule.executeJavaScriptAndWaitForResult(
                                awContents, contentClient, "outerWidth"));
        float innerWidth =
                Integer.parseInt(
                        mActivityTestRule.executeJavaScriptAndWaitForResult(
                                awContents, contentClient, "innerWidth"));
        Assert.assertEquals(innerWidth * dipScale, outerWidth, 10f);
        String deviceWidthEqualsScreenWidth =
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents,
                        contentClient,
                        "matchMedia(\"screen and (device-width:"
                                + (int) screenWidth
                                + "px)\").matches");
        Assert.assertEquals("true", deviceWidthEqualsScreenWidth);

        float screenHeight =
                Integer.parseInt(
                        mActivityTestRule.executeJavaScriptAndWaitForResult(
                                awContents, contentClient, "screen.height"));
        Assert.assertEquals(physicalDisplayHeight, screenHeight, 10f);
        float screenAvailHeight =
                Integer.parseInt(
                        mActivityTestRule.executeJavaScriptAndWaitForResult(
                                awContents, contentClient, "screen.availHeight"));
        Assert.assertEquals(physicalDisplayHeight, screenAvailHeight, 10f);
        float outerHeight =
                Integer.parseInt(
                        mActivityTestRule.executeJavaScriptAndWaitForResult(
                                awContents, contentClient, "outerHeight"));
        float innerHeight =
                Integer.parseInt(
                        mActivityTestRule.executeJavaScriptAndWaitForResult(
                                awContents, contentClient, "innerHeight"));
        Assert.assertEquals(innerHeight * dipScale, outerHeight, 10f);
        String deviceHeightEqualsScreenHeight =
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        awContents,
                        contentClient,
                        "matchMedia(\"screen and (device-height:"
                                + (int) screenHeight
                                + "px)\").matches");
        Assert.assertEquals("true", deviceHeightEqualsScreenHeight);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetaMergeContentQuirk() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSyncInQuirksMode(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        CallbackHelper onPageFinishedHelper = contentClient.getOnPageFinishedHelper();

        final int pageWidth = 3000;
        final float pageScale = 1.0f;
        final String page =
                String.format(
                        (Locale) null,
                        "<html><head><meta name='viewport' content='width=%d' /><meta"
                            + " name='viewport' content='initial-scale=%.1f' /><meta"
                            + " name='viewport' content='user-scalable=0' /></head><body"
                            + " onload='document.title=document.body.clientWidth'></body></html>",
                        pageWidth,
                        pageScale);

        settings.setJavaScriptEnabled(true);
        settings.setUseWideViewPort(true);
        settings.setBuiltInZoomControls(true);
        settings.setSupportZoom(true);

        mActivityTestRule.loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        // ContentView must update itself according to the viewport setup.
        // As we specify 'user-scalable=0', the page must become non-zoomable.
        AwActivityTestRule.pollInstrumentationThread(
                () ->
                        !mActivityTestRule.canZoomInOnUiThread(awContents)
                                && !mActivityTestRule.canZoomOutOnUiThread(awContents));
        int width = Integer.parseInt(mActivityTestRule.getTitleOnUiThread(awContents));
        Assert.assertEquals(pageWidth, width);
        Assert.assertEquals(pageScale, mActivityTestRule.getScaleOnUiThread(awContents), 0);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMetaMergeContentQuirkOverrides() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSyncInQuirksMode(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        CallbackHelper onPageFinishedHelper = contentClient.getOnPageFinishedHelper();

        final int pageWidth = 3000;
        final String page =
                String.format(
                        (Locale) null,
                        "<html><head><meta name='viewport' content='width=device-width' /><meta"
                            + " name='viewport' content='width=%d' /></head><body"
                            + " onload='document.title=document.body.clientWidth'></body></html>",
                        pageWidth);

        settings.setJavaScriptEnabled(true);
        settings.setUseWideViewPort(true);

        mActivityTestRule.loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        int width = Integer.parseInt(mActivityTestRule.getTitleOnUiThread(awContents));
        Assert.assertEquals(pageWidth, width);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testInitialScaleClobberQuirk() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSyncInQuirksMode(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        AwSettings settings = mActivityTestRule.getAwSettingsOnUiThread(awContents);
        CallbackHelper onPageFinishedHelper = contentClient.getOnPageFinishedHelper();

        final String pageTemplate =
                "<html><head>"
                        + "<meta name='viewport' content='initial-scale=%d' />"
                        + "</head><body>"
                        + "<div style='width:10000px;height:200px'>A big div</div>"
                        + "</body></html>";
        final String pageScale4 = String.format((Locale) null, pageTemplate, 4);
        final String page = String.format((Locale) null, pageTemplate, 1);

        // Page scale updates are asynchronous. There is an issue that we can't
        // reliably check, whether the scale as NOT changed (i.e. remains to be 1.0).
        // So we first change the scale to some non-default value, and then wait
        // until it gets back to 1.0.
        int onScaleChangedCallCount = contentClient.getOnScaleChangedHelper().getCallCount();
        mActivityTestRule.loadDataSync(
                awContents, onPageFinishedHelper, pageScale4, "text/html", false);
        contentClient.getOnScaleChangedHelper().waitForCallback(onScaleChangedCallCount);
        Assert.assertEquals(4.0f, mActivityTestRule.getScaleOnUiThread(awContents), 0);
        // The following call to set initial scale will be ignored. However, a temporary
        // page scale change may occur, and this makes the usual onScaleChanged-based workflow
        // flaky. So instead, we are just polling the scale until it becomes 1.0.
        settings.setInitialPageScale(50);
        mActivityTestRule.loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        ensureScaleBecomes(1.0f, awContents);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNoUserScalableQuirk() throws Throwable {
        final TestAwContentsClient contentClient = new TestAwContentsClient();
        final AwTestContainerView testContainerView =
                createAwTestContainerViewOnMainSyncInQuirksMode(contentClient);
        final AwContents awContents = testContainerView.getAwContents();
        CallbackHelper onPageFinishedHelper = contentClient.getOnPageFinishedHelper();

        final String pageScale4 =
                "<html><head>"
                        + "<meta name='viewport' content='initial-scale=4' />"
                        + "</head><body>"
                        + "<div style='width:10000px;height:200px'>A big div</div>"
                        + "</body></html>";
        final String page =
                "<html><head>"
                        + "<meta name='viewport' "
                        + "content='width=device-width,initial-scale=2,user-scalable=no' />"
                        + "</head><body>"
                        + "<div style='width:10000px;height:200px'>A big div</div>"
                        + "</body></html>";

        // Page scale updates are asynchronous. There is an issue that we can't
        // reliably check, whether the scale as NOT changed (i.e. remains to be 1.0).
        // So we first change the scale to some non-default value, and then wait
        // until it gets back to 1.0.
        int onScaleChangedCallCount = contentClient.getOnScaleChangedHelper().getCallCount();
        mActivityTestRule.loadDataSync(
                awContents, onPageFinishedHelper, pageScale4, "text/html", false);
        contentClient.getOnScaleChangedHelper().waitForCallback(onScaleChangedCallCount);
        Assert.assertEquals(4.0f, mActivityTestRule.getScaleOnUiThread(awContents), 0);
        onScaleChangedCallCount = contentClient.getOnScaleChangedHelper().getCallCount();
        mActivityTestRule.loadDataSync(awContents, onPageFinishedHelper, page, "text/html", false);
        contentClient.getOnScaleChangedHelper().waitForCallback(onScaleChangedCallCount);
        Assert.assertEquals(1.0f, mActivityTestRule.getScaleOnUiThread(awContents), 0);
    }

    private AwTestContainerView createAwTestContainerViewOnMainSyncInQuirksMode(
            final AwContentsClient client) {
        return mActivityTestRule.createAwTestContainerViewOnMainSync(client, true);
    }

    private void ensureScaleBecomes(final float targetScale, final AwContents awContents) {
        AwActivityTestRule.pollInstrumentationThread(
                () -> targetScale == mActivityTestRule.getScaleOnUiThread(awContents));
    }
}
