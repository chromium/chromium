// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.trustedwebactivity;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Intent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.InstalledWebappRegistrar;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.ui.SharedActivityCoordinator;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity.ClientPackageNameProvider;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.SplashController;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.trustedwebactivity.TwaSplashController;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.components.embedder_support.util.Origin;

import java.util.function.Supplier;

/**
 * Coordinator for the Trusted Web Activity component. Add methods here if other components need to
 * communicate with Trusted Web Activity component.
 */
@NullMarked
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

        Intent intent = intentDataProvider.getIntent();
        assert intent != null;
        boolean showSplashScreen = TwaSplashController.intentIsForTwaWithSplashScreen(intent);
        if (showSplashScreen) {
            new TwaSplashController(activity, splashControllerSupplier, intentDataProvider);
        }

        mCurrentPageVerifier.addVerificationObserver(this::onVerificationUpdate);

        LaunchMetrics.recordTWALaunch(
                assumeNonNull(intentDataProvider.getUrlToLoad()),
                intentDataProvider.getResolvedDisplayMode());
    }

    private void onVerificationUpdate() {
        CurrentPageVerifier.VerificationState state = mCurrentPageVerifier.getState();

        // The state will start off as null and progress to PENDING then SUCCESS/FAILURE. We only
        // want to register the clients once the state reaches SUCCESS.
        if (state != null && state.status == VerificationStatus.SUCCESS) {
            String packageName = mClientPackageNameProvider.get();
            Origin origin = Origin.create(state.scope);
            assert packageName != null && origin != null;
            InstalledWebappRegistrar.getInstance().registerClient(packageName, origin, state.url);
        }
    }

    /** @return The package name of the Trusted Web Activity. */
    public @Nullable String getTwaPackage() {
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
