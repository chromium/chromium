// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.webapk.lib.common.WebApkConstants;

/**
 * Unbound WebAPKs are part of Chrome. They have access to cookies and report metrics the same way
 * as the rest of Chrome. However, there is no UI indicating they are running in Chrome. For privacy
 * purposes we show a Snackbar based privacy disclosure that the activity is running as part of
 * Chrome. This occurs once per app installation, but will appear again if Chrome's storage is
 * cleared. The Snackbar must be acknowledged in order to be dismissed and should remain onscreen
 * as long as the app is open. It should remain active even across pause/resume and should show the
 * next time the app is opened if it hasn't been acknowledged.
 */
public class WebappDisclosureSnackbarController implements SnackbarManager.SnackbarController {
    /**
     * @param actionData an instance of WebappInfo
     */
    @Override
    public void onAction(Object actionData) {
        if (actionData instanceof WebappDataStorage) {
            ((WebappDataStorage) actionData).clearShowDisclosure();
        }
    }

    /**
     * Stub expected by SnackbarController.
     */
    @Override
    public void onDismissNoAction(Object actionData) {}

    /**
     * Shows the disclosure informing the user the Webapp is running in Chrome.
     * @param activity Webapp activity to show disclosure for.
     * @param storage Storage for the Webapp.
     * @param force Whether to force showing the Snackbar (if no storage is available on start).
     */
    public void maybeShowDisclosure(
            WebappActivity activity, WebappDataStorage storage, boolean force) {
        if (storage == null) return;

        // If forced we set the bit to show the disclosure. This persists to future instances.
        if (force) storage.setShowDisclosure();

        if (shouldShowDisclosure(activity, storage)) {
            activity.getSnackbarManager().showSnackbar(
                    Snackbar.make(activity.getResources().getString(
                                          R.string.app_running_in_chrome_disclosure),
                                    this, Snackbar.TYPE_PERSISTENT,
                                    Snackbar.UMA_WEBAPK_PRIVACY_DISCLOSURE)
                            .setAction(
                                    activity.getResources().getString(R.string.ok), storage)
                            .setSingleLine(false));
        }
    }

    /**
     * Restricts showing to TWAs and unbound WebAPKs that haven't dismissed the disclosure.
     * @param activity Webapp activity.
     * @param storage Storage for the Webapp.
     * @return boolean indicating whether to show the privacy disclosure.
     */
    private boolean shouldShowDisclosure(WebappActivity activity, WebappDataStorage storage) {
        // Show only if the correct flag is set.
        if (!storage.shouldShowDisclosure()) {
            return false;
        }

        // This will be null for Webapps or bound WebAPKs.
        String packageName = activity.getNativeClientPackageName();
        // Show for unbound WebAPKs.
        return packageName != null
                && !packageName.startsWith(WebApkConstants.WEBAPK_PACKAGE_PREFIX);
    }
}