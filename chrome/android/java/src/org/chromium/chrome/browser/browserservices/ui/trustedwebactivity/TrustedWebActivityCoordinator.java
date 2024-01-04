// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.trustedwebactivity;

import dagger.Lazy;

import org.chromium.chrome.browser.browserservices.InstalledWebappRegistrar;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.metrics.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.ui.SharedActivityCoordinator;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity.ClientPackageNameProvider;
import org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity.TrustedWebActivityDisclosureController;
import org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity.TrustedWebActivityOpenTimeRecorder;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.trustedwebactivity.TwaSplashController;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.components.embedder_support.util.Origin;

import javax.inject.Inject;

/**
 * Coordinator for the Trusted Web Activity component.
 * Add methods here if other components need to communicate with Trusted Web Activity component.
 */
@ActivityScope
public class TrustedWebActivityCoordinator {
    private final SharedActivityCoordinator mSharedActivityCoordinator;
    private final CurrentPageVerifier mCurrentPageVerifier;
    private final InstalledWebappRegistrar mInstalledWebappRegistrar;
    private final ClientPackageNameProvider mClientPackageNameProvider;

    @Inject
    public TrustedWebActivityCoordinator(
            SharedActivityCoordinator sharedActivityCoordinator,
            TrustedWebActivityDisclosureController disclosureController,
            DisclosureUiPicker disclosureUiPicker,
            TrustedWebActivityOpenTimeRecorder openTimeRecorder,
            CurrentPageVerifier currentPageVerifier,
            Lazy<TwaSplashController> splashController,
            BrowserServicesIntentDataProvider intentDataProvider,
            TrustedWebActivityUmaRecorder umaRecorder,
            InstalledWebappRegistrar installedWebappRegistrar,
            ClientPackageNameProvider clientPackageNameProvider) {
        // We don't need to do anything with most of the classes above, we just need to resolve them
        // so they start working.
        mSharedActivityCoordinator = sharedActivityCoordinator;
        mCurrentPageVerifier = currentPageVerifier;
        mInstalledWebappRegistrar = installedWebappRegistrar;
        mClientPackageNameProvider = clientPackageNameProvider;

        initSplashScreen(splashController, intentDataProvider, umaRecorder);

        currentPageVerifier.addVerificationObserver(this::onVerificationUpdate);
    }

    private void initSplashScreen(
            Lazy<TwaSplashController> splashController,
            BrowserServicesIntentDataProvider intentDataProvider,
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
        // want to register the clients once the state reaches SUCCESS.
        if (state != null && state.status == VerificationStatus.SUCCESS) {
            mInstalledWebappRegistrar.registerClient(
                    mClientPackageNameProvider.get(), Origin.create(state.scope), state.url);
        }
    }

    /** @return The package name of the Trusted Web Activity. */
    public String getTwaPackage() {
        return mClientPackageNameProvider.get();
    }

    /**
     * @return Whether the app is running in the "Trusted Web Activity" mode, where the TWA-specific
     *         UI is shown.
     */
    public boolean shouldUseAppModeUi() {
        return mSharedActivityCoordinator.shouldUseAppModeUi();
    }
}
