// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Build;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.features.WebappInsetsConsumer;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/** Tests for {@link DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
// The short-edges cutout paths require edge-to-edge everywhere, which is unsupported before
// Android 11.
@Config(sdk = {Build.VERSION_CODES.R, BaseRobolectricTestRunner.MAX_SDK})
public class DisplayCutoutTabHelperTest {
    // Subclass needed to stub getWebappInsetsConsumer() and getIntentDataProvider() without
    // running full ChromeActivity startup.
    private static class TestCustomTabActivity extends CustomTabActivity {
        private WebappInsetsConsumer mWebappInsetsConsumer;
        private BrowserServicesIntentDataProvider mIntentDataProvider;

        void setWebappInsetsConsumerForTesting(WebappInsetsConsumer consumer) {
            mWebappInsetsConsumer = consumer;
        }

        void setIntentDataProviderForTesting(BrowserServicesIntentDataProvider provider) {
            mIntentDataProvider = provider;
        }

        @Override
        public WebappInsetsConsumer getWebappInsetsConsumer() {
            return mWebappInsetsConsumer;
        }

        @Override
        public BrowserServicesIntentDataProvider getIntentDataProvider() {
            return mIntentDataProvider;
        }
    }

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private WindowAndroid mWindowAndroidA;
    @Mock private WindowAndroid mWindowAndroidB;
    @Mock private WindowAndroid mWindowAndroidNonCct;
    @Mock private WebappInsetsConsumer mInsetsConsumerA;
    @Mock private WebappInsetsConsumer mInsetsConsumerB;
    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;
    private TestCustomTabActivity mActivityA;
    private TestCustomTabActivity mActivityB;
    private Activity mNonCustomTabActivity;

    @Before
    public void setUp() {
        mActivityA = Robolectric.buildActivity(TestCustomTabActivity.class).get();
        mActivityA.setIntentDataProviderForTesting(mIntentDataProvider);
        when(mWindowAndroidA.getActivity()).thenReturn(new WeakReference<>(mActivityA));
        mActivityB = Robolectric.buildActivity(TestCustomTabActivity.class).get();
        when(mWindowAndroidB.getActivity()).thenReturn(new WeakReference<>(mActivityB));

        mNonCustomTabActivity = Robolectric.buildActivity(Activity.class).get();
        when(mWindowAndroidNonCct.getActivity())
                .thenReturn(new WeakReference<>(mNonCustomTabActivity));
    }

    /** Sets up {@code activity} as hosting a fullscreen web app, i.e. in the coordinated path. */
    private void setUpFullscreenWebapp(TestCustomTabActivity activity) {
        activity.setIntentDataProviderForTesting(mIntentDataProvider);
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.WEBAPP);
        when(mIntentDataProvider.getResolvedDisplayMode()).thenReturn(DisplayMode.FULLSCREEN);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    // Edge-to-edge everywhere requires R+, so on Q it is disabled regardless of flags.
    @Config(sdk = Build.VERSION_CODES.Q)
    public void testShortEdgesRequiresEdgeToEdgeEverywhere() {
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidA);
        setUpFullscreenWebapp(mActivityA);
        DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate delegate =
                new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(mTab);

        assertFalse(delegate.isShortEdgesCutoutModeEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testStandaloneShortEdgesRequiresParam() {
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidA);
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.WEBAPP);
        when(mIntentDataProvider.getResolvedDisplayMode()).thenReturn(DisplayMode.STANDALONE);
        DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate delegate =
                new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(mTab);

        assertFalse(delegate.isShortEdgesCutoutModeEnabled());
        ChromeFeatureList.sWebAppShortEdgesCutoutModeStandalone.setForTesting(true);
        assertTrue(delegate.isShortEdgesCutoutModeEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testFullscreenShortEdgesDoesNotRequireParam() {
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidA);
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.WEBAPP);
        when(mIntentDataProvider.getResolvedDisplayMode()).thenReturn(DisplayMode.FULLSCREEN);
        DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate delegate =
                new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(mTab);

        assertTrue(delegate.isShortEdgesCutoutModeEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testTwaFullscreenRequiresTwaParam() {
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidA);
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        when(mIntentDataProvider.getResolvedDisplayMode()).thenReturn(DisplayMode.FULLSCREEN);
        DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate delegate =
                new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(mTab);

        assertEquals(DisplayMode.FULLSCREEN, delegate.getDisplayMode());
        assertFalse(delegate.isShortEdgesCutoutModeEnabled());
        ChromeFeatureList.sWebAppShortEdgesCutoutModeTwa.setForTesting(true);
        assertTrue(delegate.isShortEdgesCutoutModeEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testTwaStandaloneUsesShortEdgesWithParam() {
        ChromeFeatureList.sWebAppShortEdgesCutoutModeStandalone.setForTesting(true);
        ChromeFeatureList.sWebAppShortEdgesCutoutModeTwa.setForTesting(true);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidA);
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        when(mIntentDataProvider.getResolvedDisplayMode()).thenReturn(DisplayMode.STANDALONE);
        DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate delegate =
                new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(mTab);

        assertEquals(DisplayMode.STANDALONE, delegate.getDisplayMode());
        assertTrue(delegate.isShortEdgesCutoutModeEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testMinimalUiDoesNotUseShortEdges() {
        ChromeFeatureList.sWebAppShortEdgesCutoutModeStandalone.setForTesting(true);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidA);
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.WEBAPP);
        when(mIntentDataProvider.getResolvedDisplayMode()).thenReturn(DisplayMode.MINIMAL_UI);
        DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate delegate =
                new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(mTab);

        assertFalse(delegate.isShortEdgesCutoutModeEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testBrowserDisplayModeDoesNotUseShortEdges() {
        ChromeFeatureList.sWebAppShortEdgesCutoutModeStandalone.setForTesting(true);
        // A tab in a regular browser activity reports DisplayMode.BROWSER.
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidNonCct);
        DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate delegate =
                new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(mTab);

        assertFalse(delegate.isShortEdgesCutoutModeEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testInsetsTokenRebindsAfterActivityReparenting() {
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidA);
        setUpFullscreenWebapp(mActivityA);
        setUpFullscreenWebapp(mActivityB);
        mActivityA.setWebappInsetsConsumerForTesting(mInsetsConsumerA);
        mActivityB.setWebappInsetsConsumerForTesting(mInsetsConsumerB);
        when(mInsetsConsumerA.acquireEdgeToEdgeToken()).thenReturn(1);
        DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate delegate =
                new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(mTab);

        delegate.setEdgeToEdgeState(true);
        verify(mInsetsConsumerA).acquireEdgeToEdgeToken();

        // The tab is reparented to a different BaseCustomTabActivity instance (e.g. via
        // CustomTabActivityNavigationController#openInAdjacentActivity in multi-window mode),
        // without the delegate itself being recreated.
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidB);
        delegate.setEdgeToEdgeState(true);

        // The token held against the old activity's consumer must be released, and a new token
        // acquired from the new activity's consumer. Otherwise the delegate keeps a permanent
        // reference to the old (destroyed) activity, leaking it.
        verify(mInsetsConsumerA).releaseEdgeToEdgeToken(1);
        verify(mInsetsConsumerB).acquireEdgeToEdgeToken();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testInsetsTokenReleasedWhenReparentedToNonCustomTabActivity() {
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidA);
        setUpFullscreenWebapp(mActivityA);
        mActivityA.setWebappInsetsConsumerForTesting(mInsetsConsumerA);
        when(mInsetsConsumerA.acquireEdgeToEdgeToken()).thenReturn(1);
        DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate delegate =
                new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(mTab);

        delegate.setEdgeToEdgeState(true);
        verify(mInsetsConsumerA).acquireEdgeToEdgeToken();

        // The tab is reparented into the regular (non-Custom-Tab) browser activity, e.g. via
        // "Open in Chrome" from the custom tab's overflow menu.
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidNonCct);
        delegate.setEdgeToEdgeState(true);

        verify(mInsetsConsumerA).releaseEdgeToEdgeToken(1);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testInsetsTokenHeldOnceWhileDrawingEdgeToEdge() {
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidA);
        setUpFullscreenWebapp(mActivityA);
        mActivityA.setWebappInsetsConsumerForTesting(mInsetsConsumerA);
        when(mInsetsConsumerA.acquireEdgeToEdgeToken()).thenReturn(1);
        DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate delegate =
                new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(mTab);

        delegate.setEdgeToEdgeState(true);
        delegate.setEdgeToEdgeState(true);
        verify(mInsetsConsumerA, times(1)).acquireEdgeToEdgeToken();

        delegate.setEdgeToEdgeState(false);
        delegate.setEdgeToEdgeState(false);
        verify(mInsetsConsumerA, times(1)).releaseEdgeToEdgeToken(1);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testNonParticipatingActivityHasNoInsetsConsumer() {
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidA);
        setUpFullscreenWebapp(mActivityA);
        DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate delegate =
                new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(mTab);

        delegate.setEdgeToEdgeState(true);

        verifyNoInteractions(mInsetsConsumerA);
    }
}
