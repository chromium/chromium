// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.os.Bundle;

import org.chromium.base.supplier.OneshotSupplier;

/**
 * Defines the host interface for First Run Experience pages.
 */
public interface FirstRunPageDelegate {
    /**
     * Returns FRE properties bundle.
     */
    Bundle getProperties();

    /**
     * Advances the First Run Experience to the next page.
     * Successfully finishes FRE if the current page is the last page.
     */
    void advanceToNextPage();

    /**
     * Unsuccessfully aborts the First Run Experience.
     * This usually means that the application will be closed.
     */
    void abortFirstRunExperience();

    /**
     * Successfully completes the First Run Experience.
     * All results will be packaged and sent over to the main activity.
     */
    void completeFirstRunExperience();

    /**
     * Exit the First Run Experience without marking the flow complete. This will finish the first
     * run activity and start the main activity without setting any of the preferences tracking
     * whether first run has been completed.
     *
     * Exposing this function is intended for use in scenarios where FRE is partially or completely
     * skipped. (e.g. in accordance with Enterprise polices)
     */
    void exitFirstRun();

    /**
     * Notifies that the user refused to sign in (e.g. "NO, THANKS").
     */
    void refuseSignIn();

    /**
     * Notifies that the user accepted to be signed in.
     * @param accountName An account to be signed in to.
     * @param isDefaultAccount Whether this account is the default choice for the user.
     * @param openSettings Whether the settings page should be opened after signing in.
     */
    void acceptSignIn(String accountName, boolean isDefaultAccount, boolean openSettings);

    /**
     * @return Whether the user has accepted Chrome Terms of Service.
     */
    boolean didAcceptTermsOfService();

    /**
     * Notifies all interested parties that the user has accepted Chrome Terms of Service.
     * Must be called only after native has been initialized.
     * @param allowCrashUpload True if the user allows to upload crash dumps and collect stats.
     */
    void acceptTermsOfService(boolean allowCrashUpload);

    /**
     * Show an informational web page. The page doesn't show navigation control.
     * @param url Resource id for the URL of the web page.
     */
    void showInfoPage(int url);

    /**
     * The supplier that supplies whether reading policy value is necessary.
     * See {@link PolicyLoadListener} for details.
     */
    OneshotSupplier<Boolean> getPolicyLoadListener();
}
