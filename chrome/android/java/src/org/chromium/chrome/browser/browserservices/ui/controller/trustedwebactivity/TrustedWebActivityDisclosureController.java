// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity;

import static org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel.DISCLOSURE_EVENTS_CALLBACK;
import static org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel.DISCLOSURE_FIRST_TIME;
import static org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel.DISCLOSURE_SCOPE;
import static org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel.DISCLOSURE_STATE;
import static org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel.DISCLOSURE_STATE_DISMISSED_BY_USER;
import static org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel.DISCLOSURE_STATE_NOT_SHOWN;
import static org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel.DISCLOSURE_STATE_SHOWN;
import static org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel.PACKAGE_NAME;

import org.chromium.chrome.browser.browserservices.BrowserServicesStore;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityUmaRecorder;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationState;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier.VerificationStatus;
import org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;

import javax.inject.Inject;

/**
 * Controls when Trusted Web Activity disclosure should be shown and hidden, reacts to interaction
 * with it.
 */
public class TrustedWebActivityDisclosureController
        implements NativeInitObserver, TrustedWebActivityModel.DisclosureEventsCallback {
    private final BrowserServicesStore mBrowserServicesStore;
    private final TrustedWebActivityModel mModel;
    private final CurrentPageVerifier mCurrentPageVerifier;
    private final TrustedWebActivityUmaRecorder mRecorder;
    private final ClientPackageNameProvider mClientPackageNameProvider;

    @Inject
    TrustedWebActivityDisclosureController(BrowserServicesStore browserServicesStore,
            TrustedWebActivityModel model, ActivityLifecycleDispatcher lifecycleDispatcher,
            CurrentPageVerifier currentPageVerifier, TrustedWebActivityUmaRecorder recorder,
            ClientPackageNameProvider clientPackageNameProvider) {
        mBrowserServicesStore = browserServicesStore;
        mModel = model;
        mCurrentPageVerifier = currentPageVerifier;
        mRecorder = recorder;
        mClientPackageNameProvider = clientPackageNameProvider;
        model.set(DISCLOSURE_EVENTS_CALLBACK, this);
        model.set(PACKAGE_NAME, mClientPackageNameProvider.get());
        currentPageVerifier.addVerificationObserver(this::onVerificationStatusChanged);
        lifecycleDispatcher.register(this);
    }

    private void onVerificationStatusChanged() {
        if (shouldShowInCurrentState()) {
            mModel.set(DISCLOSURE_SCOPE, mCurrentPageVerifier.getState().scope);
            showIfNeeded();
        } else {
            mModel.set(DISCLOSURE_SCOPE, null);
            dismiss();
        }
    }

    @Override
    public void onDisclosureAccepted() {
        mRecorder.recordDisclosureAccepted();
        mBrowserServicesStore.setUserAcceptedTwaDisclosureForPackage(
                mClientPackageNameProvider.get());
        mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_DISMISSED_BY_USER);
    }

    @Override
    public void onDisclosureShown() {
        mBrowserServicesStore.setUserSeenTwaDisclosureForPackage(mClientPackageNameProvider.get());
        mModel.set(DISCLOSURE_FIRST_TIME, false);
    }

    /** Shows the disclosure if it is not already showing and hasn't been accepted. */
    private void showIfNeeded() {
        if (!isShowing() && !wasDismissed()) {
            mRecorder.recordDisclosureShown();
            mModel.set(DISCLOSURE_FIRST_TIME, isFirstTime());
            mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_SHOWN);
        }
    }

    /** Dismisses the disclosure if it is showing. */
    private void dismiss() {
        if (isShowing()) {
            mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_NOT_SHOWN);
        }
    }

    /** Has a disclosure been dismissed for this client package before? */
    private boolean wasDismissed() {
        return mBrowserServicesStore.hasUserAcceptedTwaDisclosureForPackage(
                mClientPackageNameProvider.get());
    }

    /** Is this the first time the user has seen the disclosure? */
    private boolean isFirstTime() {
        return !mBrowserServicesStore.hasUserSeenTwaDisclosureForPackage(
                mClientPackageNameProvider.get());
    }

    @Override
    public void onFinishNativeInitialization() {
        // We want to show disclosure ASAP, which is limited by SnackbarManager requiring native.
        if (shouldShowInCurrentState()) {
            showIfNeeded();
        }
    }

    private boolean shouldShowInCurrentState() {
        VerificationState state = mCurrentPageVerifier.getState();
        return state != null && state.status != VerificationStatus.FAILURE;
    }

    public boolean isShowing() {
        return mModel.get(DISCLOSURE_STATE) == DISCLOSURE_STATE_SHOWN;
    }
}
