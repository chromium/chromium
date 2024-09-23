// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.os.Bundle;

import org.chromium.base.Promise;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.ui.base.WindowAndroid;

/** Defines the host interface for First Run Experience pages. */
public interface FirstRunPageDelegate {
    /** Returns FRE properties bundle. */
    Bundle getProperties();

    /**
     * Advances the First Run Experience to the next page.
     * Successfully finishes FRE if the current page is the last page.
     * @return Whether advancing to the next page succeeded.
     */
    boolean advanceToNextPage();

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
     * @return Whether the user has accepted Chrome Terms of Service.
     */
    boolean didAcceptTermsOfService();

    /** Returns whether chrome is launched as a custom tab. */
    boolean isLaunchedFromCct();

    /**
     * Notifies all interested parties that the user has accepted Chrome Terms of Service.
     * Must be called only after the delegate has fully initialized.
     * Does not automatically advance to the next page, call {@link #advanceToNextPage()} directly.
     * @param allowMetricsAndCrashUploading True if the user allows to upload crash dumps and
     *         collect stats.
     */
    void acceptTermsOfService(boolean allowMetricsAndCrashUploading);

    /**
     * Show an informational web page. The page doesn't show navigation control.
     * @param url Resource id for the URL of the web page.
     */
    void showInfoPage(int url);

    /**
     * Records the FRE progress histogram MobileFre.Progress.*.
     * @param state FRE state to record.
     */
    void recordFreProgressHistogram(@MobileFreProgress int state);

    /** Records MobileFre.FromLaunch.NativeAndPoliciesLoaded histogram. **/
    void recordNativePolicyAndChildStatusLoadedHistogram();

    /** Records MobileFre.FromLaunch.NativeInitialized histogram. **/
    void recordNativeInitializedHistogram();

    /**
     * @return The supplier that provides the Profile (when available).
     */
    OneshotSupplier<ProfileProvider> getProfileProviderSupplier();

    /**
     * The supplier that supplies whether reading policy value is necessary.
     * See {@link PolicyLoadListener} for details.
     */
    OneshotSupplier<Boolean> getPolicyLoadListener();

    /** Returns the supplier that supplies child account status. */
    OneshotSupplier<Boolean> getChildAccountStatusSupplier();

    /**
     * Returns the promise that provides information about native initialization. Callers can use
     * {@link Promise#isFulfilled()} to check whether the native has already been initialized.
     */
    Promise<Void> getNativeInitializationPromise();

    /** Return the {@link WindowAndroid} for the FirstRunActivity. */
    WindowAndroid getWindowAndroid();
}
