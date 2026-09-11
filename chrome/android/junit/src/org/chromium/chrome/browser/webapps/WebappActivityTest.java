// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;

import androidx.browser.customtabs.CustomTabsIntent;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.test.WebApkTestHelper;

/** Unit tests for {@link WebappActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 33)
public class WebappActivityTest {
    private static class TestWebappActivity extends WebappActivity {
        private @Nullable BrowserServicesIntentDataProvider mMockIntentDataProvider;

        void setMockIntentDataProvider(@Nullable BrowserServicesIntentDataProvider provider) {
            mMockIntentDataProvider = provider;
        }

        @Override
        public @Nullable BrowserServicesIntentDataProvider getIntentDataProvider() {
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

        @Nullable Drawable callGetBackgroundDrawable() {
            return getBackgroundDrawable();
        }
    }

    private static class TestSameTaskWebApkActivity extends SameTaskWebApkActivity {
        private @Nullable BrowserServicesIntentDataProvider mMockIntentDataProvider;

        void setMockIntentDataProvider(@Nullable BrowserServicesIntentDataProvider provider) {
            mMockIntentDataProvider = provider;
        }

        @Override
        public @Nullable BrowserServicesIntentDataProvider getIntentDataProvider() {
            return mMockIntentDataProvider != null
                    ? mMockIntentDataProvider
                    : super.getIntentDataProvider();
        }

        @Nullable Drawable callGetBackgroundDrawable() {
            return getBackgroundDrawable();
        }
    }

    private WebappExtras createWebappExtras(
            @Nullable Integer backgroundColor, int defaultBackgroundColor) {
        return new WebappExtras(
                "id",
                "https://example.com",
                "https://example.com",
                new WebappIcon(),
                "name",
                "shortName",
                DisplayMode.STANDALONE,
                0,
                0,
                backgroundColor,
                null,
                defaultBackgroundColor,
                false,
                false,
                false);
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

    @Test
    public void getBackgroundDrawable_withCustomBackgroundColor() {
        TestWebappActivity activity = new TestWebappActivity();
        BrowserServicesIntentDataProvider intentDataProvider =
                mock(BrowserServicesIntentDataProvider.class);
        // Semi-transparent green (0x8000FF00) should be converted to opaque green (0xFF00FF00).
        WebappExtras webappExtras = createWebappExtras(0x8000FF00, Color.WHITE);
        when(intentDataProvider.getWebappExtras()).thenReturn(webappExtras);
        activity.setMockIntentDataProvider(intentDataProvider);

        Drawable drawable = activity.callGetBackgroundDrawable();
        assertNotNull(drawable);
        assertTrue(drawable instanceof ColorDrawable);
        assertEquals(Color.GREEN, ((ColorDrawable) drawable).getColor());
    }

    @Test
    public void getBackgroundDrawable_withDefaultBackgroundColorFallback() {
        TestWebappActivity activity = new TestWebappActivity();
        BrowserServicesIntentDataProvider intentDataProvider =
                mock(BrowserServicesIntentDataProvider.class);
        WebappExtras webappExtras = createWebappExtras(null, Color.BLUE);
        when(intentDataProvider.getWebappExtras()).thenReturn(webappExtras);
        activity.setMockIntentDataProvider(intentDataProvider);

        Drawable drawable = activity.callGetBackgroundDrawable();
        assertNotNull(drawable);
        assertTrue(drawable instanceof ColorDrawable);
        assertEquals(Color.BLUE, ((ColorDrawable) drawable).getColor());
    }

    @Test
    @Config(sdk = 30)
    public void getBackgroundDrawable_sameTaskWebApkActivity_preS_returnsNull() {
        TestSameTaskWebApkActivity activity = new TestSameTaskWebApkActivity();
        BrowserServicesIntentDataProvider intentDataProvider =
                mock(BrowserServicesIntentDataProvider.class);
        WebappExtras webappExtras = createWebappExtras(Color.RED, Color.WHITE);
        when(intentDataProvider.getWebappExtras()).thenReturn(webappExtras);
        activity.setMockIntentDataProvider(intentDataProvider);

        Drawable drawable = activity.callGetBackgroundDrawable();
        org.junit.Assert.assertNull(drawable);
    }

    @Test
    @Config(sdk = 31)
    public void getBackgroundDrawable_sameTaskWebApkActivity_sPlus_returnsColorDrawable() {
        TestSameTaskWebApkActivity activity = new TestSameTaskWebApkActivity();
        BrowserServicesIntentDataProvider intentDataProvider =
                mock(BrowserServicesIntentDataProvider.class);
        WebappExtras webappExtras = createWebappExtras(Color.RED, Color.WHITE);
        when(intentDataProvider.getWebappExtras()).thenReturn(webappExtras);
        activity.setMockIntentDataProvider(intentDataProvider);

        Drawable drawable = activity.callGetBackgroundDrawable();
        assertNotNull(drawable);
        assertTrue(drawable instanceof ColorDrawable);
        assertEquals(Color.RED, ((ColorDrawable) drawable).getColor());
    }
}
