// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.os.Build;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browserservices.InstalledWebappRegistrar;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.permissiondelegation.PermissionUpdater;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.components.embedder_support.util.Origin;

import java.util.function.Supplier;

/**
 * Coordinator for the WebAPK activity component. Add methods here if other components need to
 * communicate with the WebAPK activity component.
 */
@NullMarked
public class WebApkActivityCoordinator implements DestroyObserver {
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final Supplier<WebApkUpdateManager> mWebApkUpdateManager;

    public WebApkActivityCoordinator(
            BrowserServicesIntentDataProvider intentDataProvider,
            Supplier<WebApkUpdateManager> webApkUpdateManager,
            WebappDeferredStartupWithStorageHandler webappDeferredStartupWithStorageHandler,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mIntentDataProvider = intentDataProvider;
        mWebApkUpdateManager = webApkUpdateManager;

        webappDeferredStartupWithStorageHandler.addTask(
                (storage, didCreateStorage) -> {
                    if (lifecycleDispatcher.isActivityFinishingOrDestroyed()) {
                        return;
                    }

                    assert storage != null;
                    onDeferredStartupWithStorage(storage, didCreateStorage);
                });
        lifecycleDispatcher.register(this);
    }

    public void onDeferredStartupWithStorage(WebappDataStorage storage, boolean didCreateStorage) {
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
        assert origin != null;
        assert packageName != null;

        InstalledWebappRegistrar.getInstance()
                .registerClient(packageName, origin, storage.getUrl());
        PermissionUpdater.onWebApkLaunch(origin, packageName);
    }

    @Override
    public void onDestroy() {
        // The common case is to be connected to just one WebAPK's services. For the sake of
        // simplicity disconnect from the services of all WebAPKs.
        ChromeWebApkHost.disconnectFromAllServices(/* waitForPendingWork= */ true);
    }
}
