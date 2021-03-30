// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.ui.controller.webapps.WebappDisclosureController;
import org.chromium.chrome.browser.browserservices.ui.view.DisclosureInfobar;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Coordinator for the WebAPK activity component.
 * Add methods here if other components need to communicate with the WebAPK activity component.
 */
@ActivityScope
public class WebApkActivityCoordinator implements Destroyable {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final Lazy<WebApkUpdateManager> mWebApkUpdateManager;

    @Inject
    public WebApkActivityCoordinator(
            WebappDeferredStartupWithStorageHandler deferredStartupWithStorageHandler,
            WebappDisclosureController disclosureController, DisclosureInfobar disclosureInfobar,
            WebApkActivityLifecycleUmaTracker webApkActivityLifecycleUmaTracker,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            BrowserServicesIntentDataProvider intendDataProvider,
            Lazy<WebApkUpdateManager> webApkUpdateManager) {
        // We don't need to do anything with |disclosureController|, |disclosureInfobar| and
        // |webApkActivityLifecycleUmaTracker|. We just need to resolve
        // them so that they start working.

        mIntentDataProvider = intendDataProvider;
        mWebApkUpdateManager = webApkUpdateManager;

        deferredStartupWithStorageHandler.addTask((storage, didCreateStorage) -> {
            if (lifecycleDispatcher.isActivityFinishingOrDestroyed()) return;

            onDeferredStartupWithStorage(storage, didCreateStorage);
        });
        lifecycleDispatcher.register(this);
    }

    public void onDeferredStartupWithStorage(
            @NonNull WebappDataStorage storage, boolean didCreateStorage) {
        assert storage != null;
        storage.incrementLaunchCount();

        mWebApkUpdateManager.get().updateIfNeeded(storage, mIntentDataProvider);
    }

    @Override
    public void destroy() {
        // The common case is to be connected to just one WebAPK's services. For the sake of
        // simplicity disconnect from the services of all WebAPKs.
        ChromeWebApkHost.disconnectFromAllServices(true /* waitForPendingWork */);
    }
}
