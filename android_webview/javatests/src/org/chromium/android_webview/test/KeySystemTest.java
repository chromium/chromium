// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.permission.AwPermissionRequest;
import org.chromium.android_webview.permission.Resource;
import org.chromium.base.test.util.Feature;

/**
 * TestSuite for EME key systems.
 *
 * MediaDrm support requires KitKat or later.
 * Although, WebView requires Lollipop for the onPermissionRequest() API,
 * this test intercepts this path and thus can run on KitKat.
 */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class KeySystemTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    /**
     * AwContentsClient subclass that allows permissions requests for the
     * protected media ID. This is required for all supported key systems other
     * than Clear Key.
     */
    private static class EmeAllowingAwContentsClient extends TestAwContentsClient {
        @Override
        public void onPermissionRequest(AwPermissionRequest awPermissionRequest) {
            if (awPermissionRequest.getResources() == Resource.PROTECTED_MEDIA_ID) {
                awPermissionRequest.grant();
            } else {
                awPermissionRequest.deny();
            }
        }
    }

    private TestAwContentsClient mContentsClient = new EmeAllowingAwContentsClient();
    private AwContents mAwContents;

    public KeySystemTest(AwSettingsMutation param) {
        this.mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Exception {
        final AwTestContainerView testContainerView =
                mActivityTestRule.createAwTestContainerViewOnMainSync(mContentsClient);
        mAwContents = testContainerView.getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);

        mActivityTestRule.loadUrlSync(
                mAwContents,
                mContentsClient.getOnPageFinishedHelper(),
                "file:///android_asset/key-system-test.html");
    }

    private String isKeySystemSupported(String keySystem) throws Exception {
        mActivityTestRule.executeJavaScriptAndWaitForResult(
                mAwContents, mContentsClient, "isKeySystemSupported('" + keySystem + "')");

        AwActivityTestRule.pollInstrumentationThread(() -> !getResultFromJS().equals("null"));

        return getResultFromJS();
    }

    private boolean areProprietaryCodecsSupported() throws Exception {
        String result =
                mActivityTestRule.maybeStripDoubleQuotes(
                        mActivityTestRule.executeJavaScriptAndWaitForResult(
                                mAwContents, mContentsClient, "areProprietaryCodecsSupported()"));
        return !result.isEmpty();
    }

    private String getResultFromJS() {
        String result = "null";
        try {
            result =
                    mActivityTestRule.executeJavaScriptAndWaitForResult(
                            mAwContents, mContentsClient, "result");
        } catch (Exception e) {
            throw new AssertionError("Unable to get result", e);
        }
        return result;
    }

    private String getPlatformKeySystemExpectations() throws Exception {
        // isKeySystemSupported() calls navigator.requestMediaKeySystemAccess()
        // with a video/mp4 configuration. mp4 is only supported if
        // areProprietaryCodecsSupported().
        if (areProprietaryCodecsSupported()) {
            return "\"supported\"";
        }

        return "\"NotSupportedError\"";
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testSupportClearKeySystem() throws Throwable {
        // Clear Key is always supported. However, isKeySystemSupported()
        // specifies a video/mp4 configuration, so it only succeeds if
        // proprietary codecs are supported.
        Assert.assertEquals(
                getPlatformKeySystemExpectations(), isKeySystemSupported("org.w3.clearkey"));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testSupportWidevineKeySystem() throws Throwable {
        Assert.assertEquals(
                getPlatformKeySystemExpectations(), isKeySystemSupported("com.widevine.alpha"));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testNotSupportFooKeySystem() throws Throwable {
        Assert.assertEquals("\"NotSupportedError\"", isKeySystemSupported("com.foo.keysystem"));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testSupportPlatformKeySystem() throws Throwable {
        Assert.assertEquals(
                getPlatformKeySystemExpectations(),
                isKeySystemSupported("x-com.oem.test-keysystem"));
    }

    @Test
    @Feature({"AndroidWebView"})
    @SmallTest
    public void testSupportPlatformKeySystemNoPrefix() throws Throwable {
        Assert.assertEquals(
                "\"NotSupportedError\"", isKeySystemSupported("com.oem.test-keysystem"));
    }
}
