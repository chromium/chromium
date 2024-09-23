// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.controller.webapps;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.DisclosureController;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.webapps.WebappDataStorage;
import org.chromium.chrome.browser.webapps.WebappDeferredStartupWithStorageHandler;
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
public class WebappDisclosureController extends DisclosureController {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;

    @Inject
    public WebappDisclosureController(
            BrowserServicesIntentDataProvider intentDataProvider,
            WebappDeferredStartupWithStorageHandler deferredStartupWithStorageHandler,
            TrustedWebActivityModel model,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            CurrentPageVerifier currentPageVerifier) {
        super(
                model,
                lifecycleDispatcher,
                currentPageVerifier,
                intentDataProvider.getClientPackageName());
        mIntentDataProvider = intentDataProvider;

        deferredStartupWithStorageHandler.addTask(
                (storage, didCreateStorage) -> {
                    if (lifecycleDispatcher.isActivityFinishingOrDestroyed()) return;

                    onDeferredStartupWithStorage(storage, didCreateStorage);
                });
    }

    void onDeferredStartupWithStorage(
            @Nullable WebappDataStorage storage, boolean didCreateStorage) {
        if (didCreateStorage && storage != null) {
            // SetShowDisclosure to indicate that we need to show a privacy disclosure for the newly
            // installed unbound WebAPKs which have no storage yet. We can't simply default to a
            // showing if the storage has a default value as we don't want to show this disclosure
            // for pre-existing unbound WebAPKs.
            storage.setShowDisclosure();
            if (shouldShowInCurrentState()) {
                showIfNeeded();
            }
        }
    }

    @Override
    public void onDisclosureAccepted() {
        WebappDataStorage storage =
                WebappRegistry.getInstance()
                        .getWebappDataStorage(mIntentDataProvider.getWebappExtras().id);
        assert storage != null;

        storage.clearShowDisclosure();
        super.onDisclosureAccepted();
    }

    /**
     * Restricts showing to unbound WebAPKs that haven't dismissed the disclosure.
     * @return boolean indicating whether to show the privacy disclosure.
     */
    @Override
    protected boolean shouldShowDisclosure() {
        // Only show disclosure for unbound WebAPKs.
        if (mIntentDataProvider.getClientPackageName() == null
                || mIntentDataProvider
                        .getClientPackageName()
                        .startsWith(WebApkConstants.WEBAPK_PACKAGE_PREFIX)) {
            return false;
        }

        WebappDataStorage storage =
                WebappRegistry.getInstance()
                        .getWebappDataStorage(mIntentDataProvider.getWebappExtras().id);
        if (storage == null) return false;

        // Show only if the correct flag is set.
        return storage.shouldShowDisclosure();
    }

    @Override
    protected boolean isFirstTime() {
        // TODO(crbug.com/40149064): isFirstTime is used for showing notification disclosure for
        // TWAs, not used in Webapk for now.
        return false;
    }
}
