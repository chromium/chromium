// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.browser.customtabs.CustomTabsIntent;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

/** Unit tests for {@link WebappActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, sdk = 33)
public class WebappActivityTest {
    private static class TestWebappActivity extends WebappActivity {
        private BrowserServicesIntentDataProvider mMockIntentDataProvider;

        void setMockIntentDataProvider(BrowserServicesIntentDataProvider provider) {
            mMockIntentDataProvider = provider;
        }

        @Override
        public BrowserServicesIntentDataProvider getIntentDataProvider() {
            return mMockIntentDataProvider != null
                    ? mMockIntentDataProvider
                    : super.getIntentDataProvider();
        }

        BrowserServicesIntentDataProvider callBuildIntentDataProvider(Intent intent) {
            return buildIntentDataProvider(intent, CustomTabsIntent.COLOR_SCHEME_LIGHT);
        }

        boolean callShouldDrawEdgeToEdgeOnCreate() {
            return shouldDrawEdgeToEdgeOnCreate();
        }

        boolean callCanColorStatusBarWithEdgeToEdgeHelper() {
            return canColorStatusBarWithEdgeToEdgeHelper();
        }

        boolean callCanSetTransparentStatusBarWithoutDelegate() {
            return canSetTransparentStatusBarWithoutDelegate();
        }
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE)
    @Features.DisableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void shouldDrawEdgeToEdgeOnCreateWithShortEdgesDisabled() {
        TestWebappActivity activity = new TestWebappActivity();

        assertTrue(activity.callShouldDrawEdgeToEdgeOnCreate());
        assertTrue(activity.callCanColorStatusBarWithEdgeToEdgeHelper());
        assertFalse(activity.callCanSetTransparentStatusBarWithoutDelegate());
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE,
        ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE
    })
    public void shouldNotDrawEdgeToEdgeOnCreateWithShortEdgesEnabled() {
        TestWebappActivity activity = new TestWebappActivity();

        assertFalse(activity.callShouldDrawEdgeToEdgeOnCreate());
        assertTrue(activity.callCanColorStatusBarWithEdgeToEdgeHelper());
        assertTrue(activity.callCanSetTransparentStatusBarWithoutDelegate());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    @Features.DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE)
    public void shortEdgesCanColorStatusBarWithoutDelegate() {
        TestWebappActivity activity = new TestWebappActivity();

        assertFalse(activity.callShouldDrawEdgeToEdgeOnCreate());
        assertTrue(activity.callCanColorStatusBarWithEdgeToEdgeHelper());
        assertTrue(activity.callCanSetTransparentStatusBarWithoutDelegate());
    }

    @Test
    public void testBuildIntentDataProvider_inheritsWebApkPackageNameAndDataUrlForDeepLink() {
        WebApkValidator.setDisableValidationForTesting(true);

        String webApkPackage = "org.chromium.webapk.test_package";
        String startUrl = "https://www.google.com/scope/start";
        String deepLinkUrl = "https://www.google.com/scope/deeplink";

        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, startUrl);
        bundle.putString(WebApkMetaDataKeys.SCOPE, "https://www.google.com/scope/");
        WebApkTestHelper.registerWebApkWithMetaData(
                webApkPackage, bundle, /* shareTargetMetaData= */ null);

        // Simulate an existing WebApk IntentDataProvider on the running activity.
        BrowserServicesIntentDataProvider existingProvider =
                WebApkIntentDataProviderFactory.create(
                        WebApkTestHelper.createMinimalWebApkIntent(webApkPackage, startUrl));
        assertNotNull(existingProvider);

        TestWebappActivity activity = new TestWebappActivity();
        activity.setMockIntentDataProvider(existingProvider);

        // Send a raw Android VIEW Intent (no Chrome extras, only data URL).
        Intent rawDeepLinkIntent = new Intent(Intent.ACTION_VIEW, Uri.parse(deepLinkUrl));

        BrowserServicesIntentDataProvider result =
                activity.callBuildIntentDataProvider(rawDeepLinkIntent);

        assertNotNull(result);
        assertTrue(result.isWebApkActivity());
        assertNotNull(result.getWebApkExtras());
        assertEquals(webApkPackage, result.getWebApkExtras().webApkPackageName);
        assertEquals(deepLinkUrl, result.getUrlToLoad());
        assertNotNull(result.getWebappExtras());
        assertTrue(result.getWebappExtras().shouldForceNavigation);
    }

    @Test
    public void testBuildIntentDataProvider_withoutExistingProviderReturnsNullForRawIntent() {
        TestWebappActivity activity = new TestWebappActivity();

        // Send a raw Android VIEW Intent when activity has no existing data provider.
        Intent rawDeepLinkIntent =
                new Intent(Intent.ACTION_VIEW, Uri.parse("https://www.google.com/scope/deeplink"));

        BrowserServicesIntentDataProvider result =
                activity.callBuildIntentDataProvider(rawDeepLinkIntent);

        // Should return null because raw intent has neither WebAPK package name nor legacy ID.
        org.junit.Assert.assertNull(result);
    }
}
