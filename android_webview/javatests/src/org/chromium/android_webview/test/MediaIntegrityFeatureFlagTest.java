// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.test.util.TestWebServer;

/**
 * Tests for the various permutations of feature flags for the Android WebView Media Integrity API
 * to ensure that the various permutations of the flags all behave correctly.
 *
 * <p>For more in-depth test of the API, see {@link AwMediaIntegrityApiTest}.
 */
@DoNotBatch(reason = "Heavy feature manipulation.")
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class MediaIntegrityFeatureFlagTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    private TestAwContentsClient mContentsClient;
    private AwContents mAwContents;

    public MediaIntegrityFeatureFlagTest(AwSettingsMutation param) {
        mActivityTestRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() throws Throwable {
        mContentsClient = new TestAwContentsClient();
        mAwContents =
                mActivityTestRule
                        .createAwTestContainerViewOnMainSync(mContentsClient)
                        .getAwContents();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwContents);
        // TODO(crbug.com/330151742): AWMI doesn't use the origin of the base URL set by loads from
        // loadDataWithBaseUrl. For now, use a TestWebServer to load a default HTTPS page.
        try (TestWebServer server = TestWebServer.startSsl()) {
            String url = server.setEmptyResponse("");
            mActivityTestRule.loadUrlSync(
                    mAwContents, mContentsClient.getOnPageFinishedHelper(), url);
        }
    }

    // Note that this doesn't make assertions about the non-existence of [window.]android or
    // [window.]android.webview. android or android.webview could be present without the media
    // integrity feature if that blink runtime feature is enabled separately.
    //
    // This specifically tests for nativeness to avoid confusion with the older
    // WEBVIEW_MEDIA_INTEGRITY_API feature's implementation, which may or may not be available even
    // if the feature is enabled, depending on build configuration.
    private void assertWhetherApiVisibleAndNative(boolean shouldEqual) throws Throwable {
        final String script =
                ""
                        + "try { "
                        + "  android.webview.getExperimentalMediaIntegrityTokenProvider.toString()"
                        + "} catch (e) {"
                        + "  e.toString()"
                        + "}";
        final String result =
                mActivityTestRule.executeJavaScriptAndWaitForResult(
                        mAwContents, mContentsClient, script);
        final String expectedIfNative =
                "\"function getExperimentalMediaIntegrityTokenProvider() { [native code] }\"";
        if (shouldEqual) {
            Assert.assertEquals(expectedIfNative, result);
        } else {
            Assert.assertNotEquals(expectedIfNative, result);
        }
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add({
        "disable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API,
        "disable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testNeitherFeatureEnabledHidesNativeApis() throws Throwable {
        assertWhetherApiVisibleAndNative(false);
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API,
        "disable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testOnlyOriginalFeatureEnabledHidesNativeApis() throws Throwable {
        assertWhetherApiVisibleAndNative(false);
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add({
        "disable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API,
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testOnlyBlinkExtensionFeatureEnabledExposesNativeApis() throws Throwable {
        assertWhetherApiVisibleAndNative(true);
    }

    @Test
    @LargeTest
    @CommandLineFlags.Add({
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API,
        "enable-features=" + AwFeatures.WEBVIEW_MEDIA_INTEGRITY_API_BLINK_EXTENSION
    })
    public void testBothFeaturesEnabledHidesNativeApis() throws Throwable {
        assertWhetherApiVisibleAndNative(false);
    }
}
