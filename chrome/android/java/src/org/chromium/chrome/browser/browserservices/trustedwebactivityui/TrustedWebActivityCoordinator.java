// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.trustedwebactivityui;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.Origin;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.ClientPackageNameProvider;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityBrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityDisclosureController;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityOpenTimeRecorder;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TwaRegistrar;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.Verifier;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.splashscreen.TwaSplashController;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.view.TrustedWebActivityDisclosureView;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabStatusBarColorProvider;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.features.ImmersiveModeController;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Coordinator for the Trusted Web Activity component.
 * Add methods here if other components need to communicate with Trusted Web Activity component.
 */
@ActivityScope
public class TrustedWebActivityCoordinator implements InflationObserver {

    private final CurrentPageVerifier mCurrentPageVerifier;
    private TrustedWebActivityBrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private final CustomTabStatusBarColorProvider mStatusBarColorProvider;
    private final Lazy<ImmersiveModeController> mImmersiveModeController;
    private final TwaRegistrar mTwaRegistrar;
    private final ClientPackageNameProvider mClientPackageNameProvider;

    private boolean mInTwaMode = true;

    @Inject
    public TrustedWebActivityCoordinator(
            TrustedWebActivityDisclosureController disclosureController,
            TrustedWebActivityDisclosureView disclosureView,
            TrustedWebActivityOpenTimeRecorder openTimeRecorder,
            CurrentPageVerifier currentPageVerifier,
            Verifier verifier,
            CustomTabActivityNavigationController navigationController,
            Lazy<TwaSplashController> splashController,
            CustomTabIntentDataProvider intentDataProvider,
            TrustedWebActivityUmaRecorder umaRecorder,
            CustomTabStatusBarColorProvider statusBarColorProvider,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            TrustedWebActivityBrowserControlsVisibilityManager browserControlsVisibilityManager,
            Lazy<ImmersiveModeController> immersiveModeController,
            TwaRegistrar twaRegistrar,
            ClientPackageNameProvider clientPackageNameProvider,
            CustomTabsConnection customTabsConnection) {
        // We don't need to do anything with most of the classes above, we just need to resolve them
        // so they start working.
        mCurrentPageVerifier = currentPageVerifier;
        mBrowserControlsVisibilityManager = browserControlsVisibilityManager;
        mStatusBarColorProvider = statusBarColorProvider;
        mImmersiveModeController = immersiveModeController;
        mTwaRegistrar = twaRegistrar;
        mClientPackageNameProvider = clientPackageNameProvider;

        navigationController.setLandingPageOnCloseCriterion(
                verifier::wasPreviouslyVerified);
        initSplashScreen(splashController, intentDataProvider, umaRecorder);

        currentPageVerifier.addVerificationObserver(this::onVerificationUpdate);
        lifecycleDispatcher.register(this);
        lifecycleDispatcher.register(
                new PostMessageDisabler(customTabsConnection, intentDataProvider));
    }

    @Override
    public void onPreInflationStartup() {
        if (mCurrentPageVerifier.getState() == null) {
            updateImmersiveMode(true); // Set immersive mode ASAP, before layout inflation.
        }
    }

    @Override
    public void onPostInflationStartup() {
        // Before the verification completes, we optimistically expect it to be successful and apply
        // the trusted web activity mode to UI.
        if (mCurrentPageVerifier.getState() == null) {
            updateUi(true);
        }
    }

    private void initSplashScreen(Lazy<TwaSplashController> splashController,
            CustomTabIntentDataProvider intentDataProvider,
            TrustedWebActivityUmaRecorder umaRecorder) {
        boolean showSplashScreen =
                TwaSplashController.intentIsForTwaWithSplashScreen(intentDataProvider.getIntent());

        if (showSplashScreen) {
            splashController.get();
        }

        umaRecorder.recordSplashScreenUsage(showSplashScreen);
    }

    private void onVerificationUpdate() {
        CurrentPageVerifier.VerificationState state = mCurrentPageVerifier.getState();

        // The state will start off as null and progress to PENDING then SUCCESS/FAILURE. We only
        // want to register the clients once the state reaches SUCCESS, however we are happy to
        // show the TWA UI while the state is null or pending.
        if (state != null && state.status == VerificationStatus.SUCCESS) {
            mTwaRegistrar.registerClient(mClientPackageNameProvider.get(),
                    Origin.create(state.scope));
        }

        boolean inTwaMode = state == null || state.status != VerificationStatus.FAILURE;
        if (inTwaMode == mInTwaMode) return;
        mInTwaMode = inTwaMode;
        updateUi(mInTwaMode);
    }

    private void updateUi(boolean inTwaMode) {
        updateImmersiveMode(inTwaMode);
        mStatusBarColorProvider.setUseTabThemeColor(inTwaMode);
        mBrowserControlsVisibilityManager.updateIsInTwaMode(inTwaMode);
    }

    private void updateImmersiveMode(boolean inTwaMode) {
        // TODO(pshmakov): implement this once we can depend on tip-of-tree of androidx-browser.
    }

    // This doesn't belong here, but doesn't deserve a separate class. Do extract it if more
    // PostMessage-related code appears.
    private static class PostMessageDisabler implements NativeInitObserver {
        private final CustomTabsConnection mCustomTabsConnection;
        private final BrowserServicesIntentDataProvider mIntentDataProvider;

        PostMessageDisabler(CustomTabsConnection connection,
                BrowserServicesIntentDataProvider intentDataProvider) {
            mCustomTabsConnection = connection;
            mIntentDataProvider = intentDataProvider;
        }

        @Override
        public void onFinishNativeInitialization() {
            if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TRUSTED_WEB_ACTIVITY_POST_MESSAGE)) {
                mCustomTabsConnection.resetPostMessageHandlerForSession(
                        mIntentDataProvider.getSession(), null);
            }
        }
    }
}
