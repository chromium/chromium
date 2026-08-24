// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity;

import static org.chromium.build.NullUtil.assertNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browserservices.BrowserServicesStore;
import org.chromium.chrome.browser.browserservices.metrics.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.DisclosureController;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;

/**
 * Controls when Trusted Web Activity disclosure should be shown and hidden, reacts to interaction
 * with it.
 */
@NullMarked
public class TrustedWebActivityDisclosureController extends DisclosureController {
    private final ClientPackageNameProvider mClientPackageNameProvider;
    private final CurrentPageVerifier mCurrentPageVerifier;
    private final WindowAndroid mWindowAndroid;

    public TrustedWebActivityDisclosureController(
            WindowAndroid windowAndroid,
            TrustedWebActivityModel model,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            CurrentPageVerifier currentPageVerifier,
            ClientPackageNameProvider clientPackageNameProvider) {
        super(model, lifecycleDispatcher, currentPageVerifier, clientPackageNameProvider.get());
        mWindowAndroid = windowAndroid;
        mClientPackageNameProvider = clientPackageNameProvider;
        mCurrentPageVerifier = currentPageVerifier;
    }

    @Override
    public void onDisclosureAccepted() {
        TrustedWebActivityUmaRecorder.recordDisclosureAccepted();
        BrowserServicesStore.setUserAcceptedTwaDisclosureForPackage(
                assertNonNull(mClientPackageNameProvider.get()));
        super.onDisclosureAccepted();
    }

    @Override
    public void onDisclosureShown() {
        TrustedWebActivityUmaRecorder.recordDisclosureShown();
        BrowserServicesStore.setUserSeenTwaDisclosureForPackage(
                assertNonNull(mClientPackageNameProvider.get()));
        super.onDisclosureShown();
    }

    @Override
    protected boolean shouldShowInCurrentState() {
        if (!DeviceFormFactor.isWindowOnTablet(mWindowAndroid)
                || !ChromeFeatureList.sDesktopAndroidTWADisclosures.isEnabled()) {
            return super.shouldShowInCurrentState();
        }
        CurrentPageVerifier.VerificationState state = mCurrentPageVerifier.getState();
        return state != null && state.status == CurrentPageVerifier.VerificationStatus.FAILURE;
    }

    @Override
    protected boolean shouldShowDisclosure() {
        /* Has a disclosure been dismissed for this client package before? */
        return !BrowserServicesStore.hasUserAcceptedTwaDisclosureForPackage(
                assertNonNull(mClientPackageNameProvider.get()));
    }

    @Override
    protected boolean isFirstTime() {
        return !BrowserServicesStore.hasUserSeenTwaDisclosureForPackage(
                assertNonNull(mClientPackageNameProvider.get()));
    }
}
