// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.trustedwebactivity;

import android.app.Activity;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browserservices.InstalledWebappRegistrar;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.ui.SharedActivityCoordinator;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity.ClientPackageNameProvider;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.SplashController;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.trustedwebactivity.TwaSplashController;
import org.chromium.components.embedder_support.util.Origin;

/**
 * Coordinator for the Trusted Web Activity component. Add methods here if other components need to
 * communicate with Trusted Web Activity component.
 */
public class TrustedWebActivityCoordinator {
    private final SharedActivityCoordinator mSharedActivityCoordinator;
    private final CurrentPageVerifier mCurrentPageVerifier;
    private final ClientPackageNameProvider mClientPackageNameProvider;

    public TrustedWebActivityCoordinator(
            Activity activity,
            SharedActivityCoordinator sharedActivityCoordinator,
            CurrentPageVerifier currentPageVerifier,
            ClientPackageNameProvider clientPackageNameProvider,
            Supplier<SplashController> splashControllerSupplier,
            BrowserServicesIntentDataProvider intentDataProvider) {
        mSharedActivityCoordinator = sharedActivityCoordinator;
        mCurrentPageVerifier = currentPageVerifier;
        mClientPackageNameProvider = clientPackageNameProvider;

        boolean showSplashScreen =
                TwaSplashController.intentIsForTwaWithSplashScreen(intentDataProvider.getIntent());
        if (showSplashScreen) {
            new TwaSplashController(activity, splashControllerSupplier, intentDataProvider);
        }

        mCurrentPageVerifier.addVerificationObserver(this::onVerificationUpdate);
    }

    private void onVerificationUpdate() {
        CurrentPageVerifier.VerificationState state = mCurrentPageVerifier.getState();

        // The state will start off as null and progress to PENDING then SUCCESS/FAILURE. We only
        // want to register the clients once the state reaches SUCCESS.
        if (state != null && state.status == VerificationStatus.SUCCESS) {
            InstalledWebappRegistrar.getInstance()
                    .registerClient(
                            mClientPackageNameProvider.get(),
                            Origin.create(state.scope),
                            state.url);
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
