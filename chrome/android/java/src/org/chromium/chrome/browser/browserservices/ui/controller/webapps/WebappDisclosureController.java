// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.webapps;

import static org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel.DISCLOSURE_EVENTS_CALLBACK;
import static org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel.DISCLOSURE_STATE;
import static org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel.DISCLOSURE_STATE_DISMISSED_BY_USER;
import static org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel.DISCLOSURE_STATE_SHOWN;
import static org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel.PACKAGE_NAME;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityModel;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.webapps.WebApkExtras;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.browser.webapps.WebappDeferredStartupWithStorageHandler;
import org.chromium.chrome.browser.webapps.WebappExtras;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.webapk.lib.common.WebApkConstants;

import javax.inject.Inject;

/**
 * Unbound WebAPKs are part of Chrome. They have access to cookies and report metrics the same way
 * as the rest of Chrome. However, there is no UI indicating they are running in Chrome. For privacy
 * purposes we show a Snackbar based privacy disclosure that the activity is running as part of
 * Chrome. This occurs once per app installation, but will appear again if Chrome's storage is
 * cleared. The Snackbar must be acknowledged in order to be dismissed and should remain onscreen
 * as long as the app is open. It should remain active even across pause/resume and should show the
 * next time the app is opened if it hasn't been acknowledged.
 */
@ActivityScope
public class WebappDisclosureController
        implements NativeInitObserver, TrustedWebActivityModel.DisclosureEventsCallback {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final TrustedWebActivityModel mModel;

    @Inject
    public WebappDisclosureController(ChromeActivity<?> activity,
            BrowserServicesIntentDataProvider intentDataProvider, TrustedWebActivityModel model,
            WebappDeferredStartupWithStorageHandler deferredStartupWithStorageHandler,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mIntentDataProvider = intentDataProvider;
        mModel = model;

        model.set(DISCLOSURE_EVENTS_CALLBACK, this);
        WebApkExtras webApkExtras = mIntentDataProvider.getWebApkExtras();
        if (webApkExtras != null && webApkExtras.webApkPackageName != null) {
            model.set(PACKAGE_NAME, webApkExtras.webApkPackageName);
        }

        lifecycleDispatcher.register(this);

        deferredStartupWithStorageHandler.addTask((storage, didCreateStorage) -> {
            if (activity.isActivityFinishingOrDestroyed()) return;

            onDeferredStartupWithStorage(storage, didCreateStorage);
        });
    }

    public void onDeferredStartupWithStorage(
            @Nullable WebappDataStorage storage, boolean didCreateStorage) {
        if (didCreateStorage) {
            // Set force == true to indicate that we need to show a privacy disclosure for the newly
            // installed unbound WebAPKs which have no storage yet. We can't simply default to a
            // showing if the storage has a default value as we don't want to show this disclosure
            // for pre-existing unbound WebAPKs.
            maybeShowDisclosure(storage, true /* force */);
        }
    }

    @Override
    public void onFinishNativeInitialization() {
        WebappExtras webappExtras = mIntentDataProvider.getWebappExtras();
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(
                mIntentDataProvider.getWebappExtras().id);
        if (storage != null) {
            maybeShowDisclosure(storage, false /* force */);
        }
    }

    @Override
    public void onDisclosureAccepted() {
        WebappExtras webappExtras = mIntentDataProvider.getWebappExtras();
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(
                mIntentDataProvider.getWebappExtras().id);
        if (storage != null) {
            storage.clearShowDisclosure();
        }
        mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_DISMISSED_BY_USER);
    }

    @Override
    public void onDisclosureShown() {}

    /**
     * Shows the disclosure informing the user the Webapp is running in Chrome.
     * @param storage Storage for the Webapp.
     * @param force Whether to force showing the Snackbar (if no storage is available on start).
     */
    private void maybeShowDisclosure(WebappDataStorage storage, boolean force) {
        if (storage == null) return;

        // If forced we set the bit to show the disclosure. This persists to future instances.
        if (force) storage.setShowDisclosure();

        if (!isShowing() && shouldShowDisclosure(storage)) {
            mModel.set(DISCLOSURE_STATE, DISCLOSURE_STATE_SHOWN);
        }
    }

    /**
     * Restricts showing to unbound WebAPKs that haven't dismissed the disclosure.
     * @param storage Storage for the Webapp.
     * @return boolean indicating whether to show the privacy disclosure.
     */
    private boolean shouldShowDisclosure(WebappDataStorage storage) {
        // Show only if the correct flag is set.
        if (!storage.shouldShowDisclosure()) {
            return false;
        }

        // Show for unbound WebAPKs.
        WebApkExtras webApkExtras = mIntentDataProvider.getWebApkExtras();
        return webApkExtras != null && webApkExtras.webApkPackageName != null
                && !webApkExtras.webApkPackageName.startsWith(
                        WebApkConstants.WEBAPK_PACKAGE_PREFIX);
    }

    public boolean isShowing() {
        return mModel.get(DISCLOSURE_STATE) == DISCLOSURE_STATE_SHOWN;
    }
}
