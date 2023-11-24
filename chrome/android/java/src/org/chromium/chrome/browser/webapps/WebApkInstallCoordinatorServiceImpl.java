// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.graphics.Bitmap;
import android.os.IBinder;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.components.version_info.VersionInfo;
import org.chromium.components.webapk_install.IOnFinishInstallCallback;
import org.chromium.components.webapk_install.IWebApkInstallCoordinatorService;

/**
 * Android service that is used to schedule WebAPK installs from Weblayer.
 * The service is currently hidden behind a FeatureFlag.
 */
public class WebApkInstallCoordinatorServiceImpl extends WebApkInstallCoordinatorService.Impl {
    private final IWebApkInstallCoordinatorService.Stub mBinder =
            new IWebApkInstallCoordinatorService.Stub() {
                @Override
                public void scheduleInstallAsync(
                        byte[] apkProto,
                        Bitmap primaryIcon,
                        boolean isPrimaryIconMaskable,
                        IOnFinishInstallCallback callback) {
                    WebApkInstallCoordinatorBridge bridge = new WebApkInstallCoordinatorBridge();
                    bridge.install(apkProto, primaryIcon, isPrimaryIconMaskable, callback);
                }
            };

    @Override
    public void onCreate() {
        ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
        super.onCreate();
    }

    @Override
    public IBinder onBind(Intent intent) {
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.WEB_APK_INSTALL_SERVICE)
                && VersionInfo.isLocalBuild()) {
            return mBinder;
        }
        return null;
    }
}
