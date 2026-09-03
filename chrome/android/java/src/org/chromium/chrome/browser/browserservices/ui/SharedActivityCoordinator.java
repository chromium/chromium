// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui;

import androidx.browser.trusted.TrustedWebActivityDisplayMode;
import androidx.browser.trusted.TrustedWebActivityDisplayMode.ImmersiveMode;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityBrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.customtabs.CustomTabOrientationController;
import org.chromium.chrome.browser.customtabs.CustomTabStatusBarColorProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.features.ImmersiveModeController;
import org.chromium.chrome.browser.customtabs.features.toolbar.BrowserServicesThemeColorProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;

import java.util.function.Supplier;

/**
 * Coordinates shared functionality between Trusted Web Activities and standalone webapps.
 *
 * <p>Responsibilities:
 *
 * <ul>
 *   <li>Decide whether app-mode UI should be shown based on current page verification state.
 *   <li>Enter and exit immersive mode when app-mode status changes.
 *   <li>Apply shared app-mode UI updates (theme usage, browser controls visibility, status bar, and
 *       orientation control).
 * </ul>
 */
@NullMarked
public class SharedActivityCoordinator implements InflationObserver {
    private final CurrentPageVerifier mCurrentPageVerifier;
    private final TrustedWebActivityBrowserControlsVisibilityManager
            mBrowserControlsVisibilityManager;
    private final CustomTabStatusBarColorProvider mStatusBarColorProvider;
    private final Supplier<ImmersiveModeController> mImmersiveModeController;
    private final CustomTabOrientationController mCustomTabOrientationController;
    private final BrowserServicesThemeColorProvider mBrowserServicesThemeColorProvider;

    private final @Nullable ImmersiveMode mImmersiveDisplayMode;

    private boolean mUseAppModeUi;

    public SharedActivityCoordinator(
            CurrentPageVerifier currentPageVerifier,
            TrustedWebActivityBrowserControlsVisibilityManager browserControlsVisibilityManager,
            CustomTabStatusBarColorProvider statusBarColorProvider,
            Supplier<ImmersiveModeController> immersiveModeController,
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabOrientationController customTabOrientationController,
            CustomTabActivityNavigationController customTabActivityNavigationController,
            Verifier verifier,
            BrowserServicesThemeColorProvider browserServicesThemeColorProvider,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mCurrentPageVerifier = currentPageVerifier;
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;
        mStatusBarColorProvider = statusBarColorProvider;
        mImmersiveModeController = immersiveModeController;
        mImmersiveDisplayMode = computeImmersiveMode(intentDataProvider);
        mCustomTabOrientationController = customTabOrientationController;
        mBrowserServicesThemeColorProvider = browserServicesThemeColorProvider;
        mUseAppModeUi = appModeUiAllowedFor(mCurrentPageVerifier.getState());

        customTabActivityNavigationController.setLandingPageOnCloseCriterion(
                verifier::wasPreviouslyVerified);

        mCurrentPageVerifier.addVerificationObserver(this::onVerificationUpdate);
        // Browser controls can become visible while staying in app mode, e.g. for a child tab
        // opened by a contextual web search; immersive mode must yield to them.
        mBrowserControlsVisibilityManager
                .getControlsVisibleSupplier()
                .addSyncObserver(visible -> updateImmersiveMode());
        lifecycleDispatcher.register(this);

        // Apply immersive mode ASAP, before layout inflation. This is a no-op unless the
        // intent's display mode is TrustedWebActivityDisplayMode.ImmersiveMode -- non-immersive
        // edge-to-edge (e.g. display: standalone webapps) is handled by DisplayCutoutController
        // once the page's viewport-fit value is known.
        updateImmersiveMode();
    }

    public boolean shouldUseAppModeUi() {
        return mUseAppModeUi;
    }

    @Override
    public void onPreInflationStartup() {}

    @Override
    public void onPostInflationStartup() {
        // Apply the best-known app-mode state after inflation. This handles the case where the
        // verifier already progressed beyond null before observers were wired.
        updateUi();
    }

    private void onVerificationUpdate() {
        boolean useAppModeUi = appModeUiAllowedFor(mCurrentPageVerifier.getState());
        if (mUseAppModeUi == useAppModeUi) return;

        mUseAppModeUi = useAppModeUi;
        updateUi();
    }

    private void updateUi() {
        updateImmersiveMode();
        mBrowserServicesThemeColorProvider.setUseTabTheme(mUseAppModeUi);
        mBrowserControlsVisibilityManager.updateIsInAppMode(mUseAppModeUi);
        mStatusBarColorProvider.setUseTabThemeColor(mUseAppModeUi);
        mCustomTabOrientationController.setCanControlOrientation(mUseAppModeUi);
    }

    // TrustedWebActivityDisplayMode.ImmersiveMode is used for TWA immersive mode and for webapps
    // with display: fullscreen. Non-immersive edge-to-edge for standalone webapps is controlled by
    // DisplayCutoutController after the page's viewport-fit value is known.
    private void updateImmersiveMode() {
        if (mImmersiveDisplayMode == null) return;

        boolean controlsVisible =
                mBrowserControlsVisibilityManager.getControlsVisibleSupplier().get();
        if (mUseAppModeUi && !controlsVisible) {
            mImmersiveModeController
                    .get()
                    .enterImmersiveMode(
                            mImmersiveDisplayMode.layoutInDisplayCutoutMode(),
                            mImmersiveDisplayMode.isSticky());
        } else {
            mImmersiveModeController.get().exitImmersiveMode();
        }
    }

    private static boolean appModeUiAllowedFor(
            CurrentPageVerifier.@Nullable VerificationState state) {
        return state == null || state.status != VerificationStatus.FAILURE;
    }

    private static @Nullable ImmersiveMode computeImmersiveMode(
            BrowserServicesIntentDataProvider intentDataProvider) {
        TrustedWebActivityDisplayMode displayMode = intentDataProvider.getProvidedTwaDisplayMode();
        return (displayMode instanceof ImmersiveMode immersiveMode) ? immersiveMode : null;
    }
}
