// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.WindowManager.LayoutParams;

import androidx.browser.trusted.TrustedWebActivityDisplayMode.DefaultMode;
import androidx.browser.trusted.TrustedWebActivityDisplayMode.ImmersiveMode;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityBrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.customtabs.CustomTabOrientationController;
import org.chromium.chrome.browser.customtabs.CustomTabStatusBarColorProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.features.ImmersiveModeController;
import org.chromium.chrome.browser.customtabs.features.toolbar.BrowserServicesThemeColorProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;

/** Tests for {@link SharedActivityCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SharedActivityCoordinatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private CurrentPageVerifier mCurrentPageVerifier;
    @Mock private TrustedWebActivityBrowserControlsVisibilityManager mBrowserControlsVisibility;
    @Mock private CustomTabStatusBarColorProvider mStatusBarColorProvider;
    @Mock private ImmersiveModeController mImmersiveModeController;
    @Mock private BrowserServicesIntentDataProvider mIntentDataProvider;
    @Mock private CustomTabOrientationController mOrientationController;
    @Mock private CustomTabActivityNavigationController mNavigationController;
    @Mock private Verifier mVerifier;
    @Mock private BrowserServicesThemeColorProvider mThemeColorProvider;
    @Mock private ActivityLifecycleDispatcher mLifecycleDispatcher;

    @Before
    public void setUp() {
        doNothing().when(mCurrentPageVerifier).addVerificationObserver(any());
        when(mIntentDataProvider.getProvidedTwaDisplayMode()).thenReturn(new DefaultMode());
        when(mBrowserControlsVisibility.getControlsVisibleSupplier())
                .thenReturn(ObservableSuppliers.createNonNull(false));
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void standaloneDoesNotEnterEdgeToEdgeBeforeViewportFitCover() {
        SharedActivityCoordinator coordinator = createCoordinator();
        coordinator.onPostInflationStartup();

        verify(mImmersiveModeController, never()).enterImmersiveMode(anyInt(), anyBoolean());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void standaloneDoesNotEnterEdgeToEdgeWithShortEdgesDisabled() {
        SharedActivityCoordinator coordinator = createCoordinator();
        coordinator.onPostInflationStartup();

        verify(mImmersiveModeController, never()).enterImmersiveMode(anyInt(), anyBoolean());
    }

    @Test
    @Features.EnableFeatures(ChromeFeatureList.WEB_APP_SHORT_EDGES_CUTOUT_MODE)
    public void immersiveModeYieldsToVisibleBrowserControls() {
        SettableNonNullObservableSupplier<Boolean> controlsVisible =
                ObservableSuppliers.createNonNull(false);
        when(mBrowserControlsVisibility.getControlsVisibleSupplier()).thenReturn(controlsVisible);
        when(mIntentDataProvider.getProvidedTwaDisplayMode())
                .thenReturn(
                        new ImmersiveMode(
                                /* isSticky= */ true,
                                LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES));

        createCoordinator();
        verify(mImmersiveModeController).enterImmersiveMode(anyInt(), anyBoolean());

        // Browser controls become visible while staying in app mode, e.g. for a child tab
        // opened by a contextual web search: immersive mode must exit.
        controlsVisible.set(true);
        verify(mImmersiveModeController).exitImmersiveMode();

        // Controls hide again (child tab closed): immersive mode returns.
        controlsVisible.set(false);
        verify(mImmersiveModeController, times(2)).enterImmersiveMode(anyInt(), anyBoolean());
    }

    private SharedActivityCoordinator createCoordinator() {
        return new SharedActivityCoordinator(
                mCurrentPageVerifier,
                mBrowserControlsVisibility,
                mStatusBarColorProvider,
                () -> mImmersiveModeController,
                mIntentDataProvider,
                mOrientationController,
                mNavigationController,
                mVerifier,
                mThemeColorProvider,
                mLifecycleDispatcher);
    }
}
