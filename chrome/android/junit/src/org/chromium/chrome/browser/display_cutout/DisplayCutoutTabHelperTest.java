// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.display_cutout;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeManager;
import org.chromium.ui.edge_to_edge.EdgeToEdgeStateProvider;

import java.lang.ref.WeakReference;

/** Tests for {@link DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@SuppressWarnings("DoNotMock") // TODO(567604165): Remove mocking of Views / Activities
public class DisplayCutoutTabHelperTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mTab;
    @Mock private WindowAndroid mWindowAndroidA;
    @Mock private WindowAndroid mWindowAndroidB;
    @Mock private WindowAndroid mWindowAndroidNonCct;
    @Mock private BaseCustomTabActivity mActivityA;
    @Mock private BaseCustomTabActivity mActivityB;
    @Mock private Activity mNonCustomTabActivity;
    @Mock private EdgeToEdgeManager mManagerA;
    @Mock private EdgeToEdgeManager mManagerB;
    @Mock private EdgeToEdgeStateProvider mProviderA;
    @Mock private EdgeToEdgeStateProvider mProviderB;
    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;

    @Before
    public void setUp() {
        when(mWindowAndroidA.getActivity()).thenReturn(new WeakReference<>(mActivityA));
        when(mActivityA.getEdgeToEdgeManager()).thenReturn(mManagerA);
        when(mManagerA.getEdgeToEdgeStateProvider()).thenReturn(mProviderA);
        when(mProviderA.acquireEdgeToEdgeToken()).thenReturn(1);

        when(mWindowAndroidB.getActivity()).thenReturn(new WeakReference<>(mActivityB));
        when(mActivityB.getEdgeToEdgeManager()).thenReturn(mManagerB);
        when(mManagerB.getEdgeToEdgeStateProvider()).thenReturn(mProviderB);
        when(mProviderB.acquireEdgeToEdgeToken()).thenReturn(2);

        when(mWindowAndroidNonCct.getActivity())
                .thenReturn(new WeakReference<>(mNonCustomTabActivity));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testStandaloneShortEdgesRequiresParam() {
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidA);
        when(mActivityA.getIntentDataProvider()).thenReturn(mIntentDataProvider);
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
        when(mActivityA.getIntentDataProvider()).thenReturn(mIntentDataProvider);
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.WEBAPP);
        when(mIntentDataProvider.getResolvedDisplayMode()).thenReturn(DisplayMode.FULLSCREEN);
        DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate delegate =
                new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(mTab);

        assertTrue(delegate.isShortEdgesCutoutModeEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testTwaFullscreenDoesNotUseShortEdges() {
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidA);
        when(mActivityA.getIntentDataProvider()).thenReturn(mIntentDataProvider);
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        when(mIntentDataProvider.getResolvedDisplayMode()).thenReturn(DisplayMode.FULLSCREEN);
        DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate delegate =
                new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(mTab);

        assertEquals(DisplayMode.FULLSCREEN, delegate.getDisplayMode());
        assertFalse(delegate.isShortEdgesCutoutModeEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testTwaStandaloneDoesNotUseShortEdgesWithParam() {
        ChromeFeatureList.sWebAppShortEdgesCutoutModeStandalone.setForTesting(true);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidA);
        when(mActivityA.getIntentDataProvider()).thenReturn(mIntentDataProvider);
        when(mIntentDataProvider.getActivityType()).thenReturn(ActivityType.TRUSTED_WEB_ACTIVITY);
        when(mIntentDataProvider.getResolvedDisplayMode()).thenReturn(DisplayMode.STANDALONE);
        DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate delegate =
                new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(mTab);

        assertEquals(DisplayMode.STANDALONE, delegate.getDisplayMode());
        assertFalse(delegate.isShortEdgesCutoutModeEnabled());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void testMinimalUiDoesNotUseShortEdges() {
        ChromeFeatureList.sWebAppShortEdgesCutoutModeStandalone.setForTesting(true);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidA);
        when(mActivityA.getIntentDataProvider()).thenReturn(mIntentDataProvider);
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
    public void testEdgeToEdgeTokenRebindsAfterActivityReparenting() {
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidA);
        DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate delegate =
                new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(mTab);

        delegate.setEdgeToEdgeState(true);
        verify(mProviderA).acquireEdgeToEdgeToken();

        // The tab is reparented to a different BaseCustomTabActivity instance (e.g. via
        // CustomTabActivityNavigationController#openInAdjacentActivity in multi-window mode),
        // without the delegate itself being recreated.
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidB);
        delegate.setEdgeToEdgeState(true);

        // The token acquired from the old activity's provider must be released, and a new token
        // must be acquired from the new activity's provider. Otherwise the delegate keeps a
        // permanent reference to the old (destroyed) activity's window, leaking it.
        verify(mProviderA).releaseEdgeToEdgeToken(1);
        verify(mProviderB).acquireEdgeToEdgeToken();
    }

    @Test
    public void testEdgeToEdgeTokenReleasedWhenReparentedToNonCustomTabActivity() {
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidA);
        DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate delegate =
                new DisplayCutoutTabHelper.ChromeDisplayCutoutDelegate(mTab);

        delegate.setEdgeToEdgeState(true);
        verify(mProviderA).acquireEdgeToEdgeToken();

        // The tab is reparented into the regular (non-Custom-Tab) browser activity, e.g. via
        // "Open in Chrome" from the custom tab's overflow menu.
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroidNonCct);
        delegate.setEdgeToEdgeState(true);

        // Even though the new activity isn't a BaseCustomTabActivity (so there's no new provider
        // to acquire a token from), the token held against the old CCT's provider must still be
        // released. Otherwise the delegate keeps pinning the old (possibly destroyed) CCT forever.
        verify(mProviderA).releaseEdgeToEdgeToken(1);
    }
}
