// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.view.Window;

import androidx.browser.customtabs.CustomTabsIntent;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.util.ReflectionHelpers;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.ColorProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.browserservices.intents.WebappIcon;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.customtabs.features.WebappInsetsConsumer;
import org.chromium.chrome.browser.customtabs.features.toolbar.BrowserServicesThemeColorProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.ui.edge_to_edge.EdgeToEdgeManager;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSystemBarColorHelper;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.webapk.test.WebApkTestHelper;

/** Unit tests for {@link WebappActivity}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 33)
public class WebappActivityTest {
    private static class TestWebappActivity extends WebappActivity {
        private @Nullable BrowserServicesIntentDataProvider mMockIntentDataProvider;
        private @Nullable EdgeToEdgeManager mMockEdgeToEdgeManager;
        private @Nullable Window mMockWindow;

        @Override
        public @Nullable EdgeToEdgeManager getEdgeToEdgeManager() {
            return mMockEdgeToEdgeManager != null
                    ? mMockEdgeToEdgeManager
                    : super.getEdgeToEdgeManager();
        }

        @Override
        public Window getWindow() {
            return mMockWindow != null ? mMockWindow : super.getWindow();
        }

        void setNavigationBarTestDependencies(EdgeToEdgeManager manager, Window window) {
            mMockEdgeToEdgeManager = manager;
            mMockWindow = window;
        }

        void callUpdateNavigationBarColor() {
            updateNavigationBarColor();
        }

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
    @Features.DisableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void activityTokenWithoutWebappInsetsConsumerUsesManifestNavigationBarColor() {
        TestWebappActivity activity = new TestWebappActivity();
        BrowserServicesIntentDataProvider provider = mock(BrowserServicesIntentDataProvider.class);
        ColorProvider colorProvider = mock(ColorProvider.class);
        EdgeToEdgeManager manager = mock(EdgeToEdgeManager.class);
        EdgeToEdgeSystemBarColorHelper colorHelper = mock(EdgeToEdgeSystemBarColorHelper.class);
        when(provider.getColorProvider()).thenReturn(colorProvider);
        when(colorProvider.getNavigationBarColor()).thenReturn(Color.RED);
        when(manager.getEdgeToEdgeSystemBarColorHelper()).thenReturn(colorHelper);
        activity.setMockIntentDataProvider(provider);
        activity.setNavigationBarTestDependencies(
                manager, Robolectric.buildActivity(Activity.class).get().getWindow());
        ReflectionHelpers.setField(
                activity,
                "mBrowserServicesThemeColorProvider",
                mock(BrowserServicesThemeColorProvider.class));

        activity.callUpdateNavigationBarColor();

        verify(colorHelper).setNavigationBarColor(Color.RED);
    }

    @Test
    public void navigationBarColorFollowsWebappInsetsConsumerTokens() {
        TestWebappActivity activity = new TestWebappActivity();
        BrowserServicesIntentDataProvider provider = mock(BrowserServicesIntentDataProvider.class);
        ColorProvider colorProvider = mock(ColorProvider.class);
        EdgeToEdgeManager manager = mock(EdgeToEdgeManager.class);
        EdgeToEdgeSystemBarColorHelper colorHelper = mock(EdgeToEdgeSystemBarColorHelper.class);
        BrowserServicesThemeColorProvider themeColorProvider =
                mock(BrowserServicesThemeColorProvider.class);
        when(provider.getColorProvider()).thenReturn(colorProvider);
        when(colorProvider.getNavigationBarColor()).thenReturn(Color.RED);
        when(manager.getEdgeToEdgeSystemBarColorHelper()).thenReturn(colorHelper);
        when(themeColorProvider.getThemeColor()).thenReturn(Color.WHITE);
        activity.setMockIntentDataProvider(provider);
        activity.setNavigationBarTestDependencies(
                manager, Robolectric.buildActivity(Activity.class).get().getWindow());
        ReflectionHelpers.setField(
                activity, "mBrowserServicesThemeColorProvider", themeColorProvider);
        WebappInsetsConsumer insetsConsumer = new WebappInsetsConsumer(mock(InsetObserver.class));
        ReflectionHelpers.setField(activity, "mWebappInsetsConsumer", insetsConsumer);

        // No token held: the manifest navigation bar color applies.
        activity.callUpdateNavigationBarColor();
        verify(colorHelper).setNavigationBarColor(Color.RED);

        // A token is held (display cutout controller or immersive mode): the bar goes
        // transparent, with the buttons following the page theme color.
        int token = insetsConsumer.acquireEdgeToEdgeToken();
        activity.callUpdateNavigationBarColor();
        verify(colorHelper).setNavigationBarColor(Color.TRANSPARENT);

        // Token released: back to the manifest color.
        insetsConsumer.releaseEdgeToEdgeToken(token);
        activity.callUpdateNavigationBarColor();
        verify(colorHelper, times(2)).setNavigationBarColor(Color.RED);
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void shortEdgesDisplayModeHelperRequiresFeature() {
        ChromeFeatureList.sWebAppShortEdgesCutoutModeStandalone.setForTesting(true);

        assertFalse(
                BaseCustomTabActivity.isShortEdgesCutoutModeEnabledForDisplayMode(
                        DisplayMode.FULLSCREEN));
        assertFalse(
                BaseCustomTabActivity.isShortEdgesCutoutModeEnabledForDisplayMode(
                        DisplayMode.STANDALONE));
    }

    @Test
    @Config(sdk = 29)
    @Features.EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void shortEdgesRequiresEdgeToEdgeEverywhere() {
        // The coordinated paths withhold insets from the edge-to-edge root layout, which does not
        // exist without edge-to-edge everywhere.
        assertFalse(
                BaseCustomTabActivity.isShortEdgesCutoutModeEnabledForDisplayMode(
                        DisplayMode.FULLSCREEN));
        assertFalse(
                BaseCustomTabActivity.isShortEdgesCutoutModeEnabledForDisplayMode(
                        DisplayMode.STANDALONE));
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
