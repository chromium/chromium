// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;

/**
 * Displays the {@link Snackbar} with provided actions for WebApk (aka PWA - Progressive Web Apps).
 *
 * <p>It checks whether the Tracking Protection {@link Snackbar} logic should be executed by looking
 * into {@link ActivityType}. If the provided {@link ActivityType} does not identify the caller as
 * WebApk, the logic won't be executed.
 */
public class TrackingProtectionSnackbarController {
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
    private final Runnable mSnakcbarOnAction;
    private SnackbarController mSnackbarController =
            new SnackbarController() {
                @Override
                public void onDismissNoAction(Object actionData) {}

                @Override
                public void onAction(Object actionData) {
                    mSnakcbarOnAction.run();
                }
            };

    /**
     * Creates the {@link TrackingProtectionSnackbarController} object.
     *
     * @param snackbarOnAction logic to be executed when action button was clicked.
     * @param snackbarManagerSupplier supplier of {@link SnackbarManager} used to generate {@link
     *     Snackbar} object when requested.
     */
    public TrackingProtectionSnackbarController(
            Runnable snackbarOnAction, Supplier<SnackbarManager> snackbarManagerSupplier) {
        mSnakcbarOnAction = snackbarOnAction;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
    }

    /**
     * Show {@link Snackbar} for TrackingProtection if the provided {@link ActivityType} is correct.
     *
     * @param activityType type of the activity.
     */
    public void showSnackbar(@ActivityType int activityType) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.TRACKING_PROTECTION_USER_BYPASS_PWA)
                || activityType != ActivityType.WEB_APK) {
            return;
        }

        Context context = ContextUtils.getApplicationContext();
        Snackbar snackbar =
                Snackbar.make(
                        context.getString(
                                R.string.privacy_sandbox_tracking_protection_snackbar_title),
                        mSnackbarController,
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_SPECIAL_LOCALE);
        snackbar.setDuration(SnackbarManager.DEFAULT_SNACKBAR_DURATION_LONG_MS);
        snackbar.setAction(
                context.getString(R.string.privacy_sandbox_tracking_protection_snackbar_action),
                null);
        mSnackbarManagerSupplier.get().showSnackbar(snackbar);
    }
}
