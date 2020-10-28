// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import org.chromium.base.UnownedUserDataKey;
import org.chromium.ui.base.WindowAndroid;

/**
 * This class manages the details associated with binding a {@link AppBannerInProductHelpController}
 * to user data on a {@link WindowAndroid}.
 */
public class AppBannerInProductHelpControllerProvider {
    /** The key used to bind the controller to the unowned data host. */
    private static final UnownedUserDataKey<AppBannerInProductHelpController> KEY =
            new UnownedUserDataKey<>(AppBannerInProductHelpController.class);

    /**
     * Get the shared {@link AppBannerInProductHelpController} from the provided {@link
     * WindowAndroid}.
     * @param windowAndroid The window to pull the controller from.
     * @return A shared instance of a {@link AppBannerInProductHelpController}.
     */
    public static AppBannerInProductHelpController from(WindowAndroid windowAndroid) {
        return KEY.retrieveDataFromHost(windowAndroid.getUnownedUserDataHost());
    }

    static void attach(WindowAndroid windowAndroid, AppBannerInProductHelpController controller) {
        KEY.attachToHost(windowAndroid.getUnownedUserDataHost(), controller);
    }

    static void detach(AppBannerInProductHelpController controller) {
        KEY.detachFromAllHosts(controller);
    }
}
