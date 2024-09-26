// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity;

import org.chromium.chrome.browser.browserservices.BrowserServicesStore;
import org.chromium.chrome.browser.browserservices.metrics.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.DisclosureController;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;

import javax.inject.Inject;

/**
 * Controls when Trusted Web Activity disclosure should be shown and hidden, reacts to interaction
 * with it.
 */
public class TrustedWebActivityDisclosureController extends DisclosureController {
    private final BrowserServicesStore mBrowserServicesStore;
    private final TrustedWebActivityUmaRecorder mRecorder;
    private final ClientPackageNameProvider mClientPackageNameProvider;

    @Inject
    TrustedWebActivityDisclosureController(
            BrowserServicesStore browserServicesStore,
            TrustedWebActivityModel model,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            CurrentPageVerifier currentPageVerifier,
            TrustedWebActivityUmaRecorder recorder,
            ClientPackageNameProvider clientPackageNameProvider) {
        super(model, lifecycleDispatcher, currentPageVerifier, clientPackageNameProvider.get());
        mBrowserServicesStore = browserServicesStore;
        mRecorder = recorder;
        mClientPackageNameProvider = clientPackageNameProvider;
    }

    @Override
    public void onDisclosureAccepted() {
        mRecorder.recordDisclosureAccepted();
        mBrowserServicesStore.setUserAcceptedTwaDisclosureForPackage(
                mClientPackageNameProvider.get());
        super.onDisclosureAccepted();
    }

    @Override
    public void onDisclosureShown() {
        mRecorder.recordDisclosureShown();
        mBrowserServicesStore.setUserSeenTwaDisclosureForPackage(mClientPackageNameProvider.get());
        super.onDisclosureShown();
    }

    @Override
    protected boolean shouldShowDisclosure() {
        /* Has a disclosure been dismissed for this client package before? */
        return !mBrowserServicesStore.hasUserAcceptedTwaDisclosureForPackage(
                mClientPackageNameProvider.get());
    }

    @Override
    protected boolean isFirstTime() {
        return !mBrowserServicesStore.hasUserSeenTwaDisclosureForPackage(
                mClientPackageNameProvider.get());
    }
}
