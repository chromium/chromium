// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.os.Build;

import androidx.annotation.NonNull;

import dagger.Lazy;

import org.chromium.chrome.browser.browserservices.InstalledWebappRegistrar;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.permissiondelegation.PermissionUpdater;
import org.chromium.chrome.browser.browserservices.ui.controller.webapps.WebappDisclosureController;
import org.chromium.chrome.browser.browserservices.ui.view.DisclosureInfobar;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.components.embedder_support.util.Origin;

import javax.inject.Inject;

/**
 * Coordinator for the WebAPK activity component. Add methods here if other components need to
 * communicate with the WebAPK activity component.
 */
@ActivityScope
public class WebApkActivityCoordinator implements DestroyObserver {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final Lazy<WebApkUpdateManager> mWebApkUpdateManager;
    private final InstalledWebappRegistrar mInstalledWebappRegistrar;

    @Inject
    public WebApkActivityCoordinator(
            WebappDeferredStartupWithStorageHandler deferredStartupWithStorageHandler,
            WebappDisclosureController disclosureController,
            DisclosureInfobar disclosureInfobar,
            WebApkActivityLifecycleUmaTracker webApkActivityLifecycleUmaTracker,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            BrowserServicesIntentDataProvider intendDataProvider,
            Lazy<WebApkUpdateManager> webApkUpdateManager,
            InstalledWebappRegistrar installedWebappRegistrar) {
        // We don't need to do anything with |disclosureController|, |disclosureInfobar| and
        // |webApkActivityLifecycleUmaTracker|. We just need to resolve
        // them so that they start working.

        mIntentDataProvider = intendDataProvider;
        mWebApkUpdateManager = webApkUpdateManager;
        mInstalledWebappRegistrar = installedWebappRegistrar;

        deferredStartupWithStorageHandler.addTask(
                (storage, didCreateStorage) -> {
                    if (lifecycleDispatcher.isActivityFinishingOrDestroyed()) return;

                    onDeferredStartupWithStorage(storage, didCreateStorage);
                });
        lifecycleDispatcher.register(this);
    }

    public void onDeferredStartupWithStorage(
            @NonNull WebappDataStorage storage, boolean didCreateStorage) {
        assert storage != null;
        storage.incrementLaunchCount();

        WebApkSyncService.onWebApkUsed(mIntentDataProvider, storage, false /* isInstall */);
        mWebApkUpdateManager.get().updateIfNeeded(storage, mIntentDataProvider);

        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) {
            return;
        }

        // The scope should not be empty here, this is for a WebAPK that just launched.
        String scope = storage.getScope();
        assert !scope.isEmpty();

        Origin origin = Origin.create(scope);
        String packageName = storage.getWebApkPackageName();

        mInstalledWebappRegistrar.registerClient(packageName, origin, storage.getUrl());
        PermissionUpdater.get().onWebApkLaunch(origin, packageName);
    }

    @Override
    public void onDestroy() {
        // The common case is to be connected to just one WebAPK's services. For the sake of
        // simplicity disconnect from the services of all WebAPKs.
        ChromeWebApkHost.disconnectFromAllServices(/* waitForPendingWork= */ true);
    }
}
