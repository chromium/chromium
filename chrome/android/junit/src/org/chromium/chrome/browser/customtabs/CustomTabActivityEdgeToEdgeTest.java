// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.content.ComponentName;
import android.content.Intent;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.browser.customtabs.TrustedWebUtils;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.ArrayList;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 33)
public class CustomTabActivityEdgeToEdgeTest {
    public static class TestCustomTabActivity extends CustomTabActivity {
        private BrowserServicesIntentDataProvider mProvider;
        private final List<BrowserServicesIntentDataProvider> mBuiltProviders = new ArrayList<>();

        void setProvider(BrowserServicesIntentDataProvider provider) {
            mProvider = provider;
        }

        @Override
        public BrowserServicesIntentDataProvider getIntentDataProvider() {
            return mProvider != null ? mProvider : super.getIntentDataProvider();
        }

        @Override
        protected int getColorScheme() {
            return CustomTabsIntent.COLOR_SCHEME_LIGHT;
        }

        @Override
        protected BrowserServicesIntentDataProvider buildIntentDataProvider(
                Intent intent, @CustomTabsIntent.ColorScheme int colorScheme) {
            BrowserServicesIntentDataProvider provider =
                    super.buildIntentDataProvider(intent, colorScheme);
            mBuiltProviders.add(provider);
            return provider;
        }

        BrowserServicesIntentDataProvider callBuildIntentDataProvider(Intent intent) {
            return buildIntentDataProvider(intent, CustomTabsIntent.COLOR_SCHEME_LIGHT);
        }

        List<BrowserServicesIntentDataProvider> getBuiltProviders() {
            return mBuiltProviders;
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

    private TestCustomTabActivity createActivity(int activityType, int displayMode) {
        TestCustomTabActivity activity = new TestCustomTabActivity();
        BrowserServicesIntentDataProvider provider = mock(BrowserServicesIntentDataProvider.class);
        when(provider.getActivityType()).thenReturn(activityType);
        when(provider.getResolvedDisplayMode()).thenReturn(displayMode);
        activity.setProvider(provider);
        return activity;
    }

    /** Creates an activity whose intent is parsed by the real startup path, as before onCreate. */
    private TestCustomTabActivity createActivityWithoutProvider(Intent intent) {
        TestCustomTabActivity activity =
                Robolectric.buildActivity(TestCustomTabActivity.class, intent).get();
        // Parsing an intent resolves toolbar colors from the activity theme.
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        return activity;
    }

    private static Intent createTwaIntent() {
        CustomTabsSession session =
                CustomTabsSession.createMockSessionForTesting(
                        new ComponentName(
                                ContextUtils.getApplicationContext(),
                                ChromeLauncherActivity.class));
        Intent intent = new CustomTabsIntent.Builder(session).build().intent;
        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true);
        return intent;
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE,
        ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE
    })
    public void twaFullscreenUsesDynamicWindowTokens() {
        ChromeFeatureList.sWebAppShortEdgesCutoutModeTwa.setForTesting(true);
        TestCustomTabActivity activity =
                createActivity(ActivityType.TRUSTED_WEB_ACTIVITY, DisplayMode.FULLSCREEN);

        assertFalse(activity.callShouldDrawEdgeToEdgeOnCreate());
        assertTrue(activity.callCanColorStatusBarWithEdgeToEdgeHelper());
        assertTrue(activity.callCanSetTransparentStatusBarWithoutDelegate());
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE,
        ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE
    })
    public void twaStandaloneRequiresOptIn() {
        ChromeFeatureList.sWebAppShortEdgesCutoutModeTwa.setForTesting(true);
        TestCustomTabActivity activity =
                createActivity(ActivityType.TRUSTED_WEB_ACTIVITY, DisplayMode.STANDALONE);

        assertTrue(activity.callShouldDrawEdgeToEdgeOnCreate());
        assertFalse(activity.callCanSetTransparentStatusBarWithoutDelegate());

        ChromeFeatureList.sWebAppShortEdgesCutoutModeStandalone.setForTesting(true);
        assertFalse(activity.callShouldDrawEdgeToEdgeOnCreate());
        assertTrue(activity.callCanSetTransparentStatusBarWithoutDelegate());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE)
    @Features.DisableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void twaFullscreenKeepsLegacyTokenWhenFeatureDisabled() {
        ChromeFeatureList.sWebAppShortEdgesCutoutModeTwa.setForTesting(true);
        TestCustomTabActivity activity =
                createActivity(ActivityType.TRUSTED_WEB_ACTIVITY, DisplayMode.FULLSCREEN);

        assertTrue(activity.callShouldDrawEdgeToEdgeOnCreate());
        assertFalse(activity.callCanSetTransparentStatusBarWithoutDelegate());
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE,
        ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE
    })
    public void twaFullscreenKeepsActivityTokenWithoutTwaParam() {
        TestCustomTabActivity activity =
                createActivity(ActivityType.TRUSTED_WEB_ACTIVITY, DisplayMode.FULLSCREEN);

        assertTrue(activity.callShouldDrawEdgeToEdgeOnCreate());
        assertFalse(activity.callCanSetTransparentStatusBarWithoutDelegate());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    @Features.DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE)
    public void twaFullscreenRequiresEdgeToEdgeEverywhere() {
        ChromeFeatureList.sWebAppShortEdgesCutoutModeTwa.setForTesting(true);
        TestCustomTabActivity activity =
                createActivity(ActivityType.TRUSTED_WEB_ACTIVITY, DisplayMode.FULLSCREEN);

        assertFalse(activity.callCanSetTransparentStatusBarWithoutDelegate());
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE,
        ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE
    })
    public void ordinaryCustomTabKeepsActivityToken() {
        ChromeFeatureList.sWebAppShortEdgesCutoutModeStandalone.setForTesting(true);
        TestCustomTabActivity activity =
                createActivity(ActivityType.CUSTOM_TAB, DisplayMode.UNDEFINED);

        assertTrue(activity.callShouldDrawEdgeToEdgeOnCreate());
        assertFalse(activity.callCanSetTransparentStatusBarWithoutDelegate());
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE,
        ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE
    })
    public void twaStartupIntentDataProviderIsReusedOnce() {
        ChromeFeatureList.sWebAppShortEdgesCutoutModeTwa.setForTesting(true);
        ChromeFeatureList.sWebAppShortEdgesCutoutModeStandalone.setForTesting(true);
        Intent startupIntent = createTwaIntent();
        TestCustomTabActivity activity = createActivityWithoutProvider(startupIntent);

        assertFalse(activity.callShouldDrawEdgeToEdgeOnCreate());
        assertTrue(activity.callCanSetTransparentStatusBarWithoutDelegate());
        assertEquals(1, activity.getBuiltProviders().size());
        BrowserServicesIntentDataProvider startupProvider = activity.getBuiltProviders().get(0);
        assertNotNull(startupProvider);
        assertTrue(startupProvider.isTrustedWebActivity());

        // Pre-inflation startup gets the cached provider back instead of parsing again.
        assertSame(startupProvider, activity.callBuildIntentDataProvider(startupIntent));
        // The cache holds a single provider, so a later build parses the intent again.
        assertNotSame(startupProvider, activity.callBuildIntentDataProvider(startupIntent));
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE,
        ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE
    })
    public void startupIntentDataProviderIsNotReusedForADifferentIntent() {
        ChromeFeatureList.sWebAppShortEdgesCutoutModeTwa.setForTesting(true);
        ChromeFeatureList.sWebAppShortEdgesCutoutModeStandalone.setForTesting(true);
        Intent startupIntent = createTwaIntent();
        TestCustomTabActivity activity = createActivityWithoutProvider(startupIntent);

        activity.callShouldDrawEdgeToEdgeOnCreate();
        BrowserServicesIntentDataProvider startupProvider = activity.getBuiltProviders().get(0);

        assertNotSame(startupProvider, activity.callBuildIntentDataProvider(createTwaIntent()));
        // The cached provider is still handed back for the intent it was parsed from.
        assertSame(startupProvider, activity.callBuildIntentDataProvider(startupIntent));
    }

    @Test
    @Features.EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE,
        ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE
    })
    public void ordinaryCustomTabIntentIsNotParsedDuringStartup() {
        ChromeFeatureList.sWebAppShortEdgesCutoutModeTwa.setForTesting(true);
        ChromeFeatureList.sWebAppShortEdgesCutoutModeStandalone.setForTesting(true);
        TestCustomTabActivity activity =
                createActivityWithoutProvider(new CustomTabsIntent.Builder().build().intent);

        assertTrue(activity.callShouldDrawEdgeToEdgeOnCreate());
        assertFalse(activity.callCanSetTransparentStatusBarWithoutDelegate());
        assertTrue(activity.getBuiltProviders().isEmpty());
    }
}
