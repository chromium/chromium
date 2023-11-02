// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.banners;

import android.app.Activity;
import android.view.View;

import androidx.annotation.IdRes;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.ui.base.WindowAndroid;

/**
 * A factory for producing a {@link AppBannerInProductHelpController}.
 */
public class AppBannerInProductHelpControllerFactory {
    public static AppBannerInProductHelpController createAppBannerInProductHelpController(
            Activity activity, AppMenuHandler appMenuHandler, Supplier<View> menuButtonView,
            @IdRes int higlightMenuItemId) {
        return new AppBannerInProductHelpController(
                activity, appMenuHandler, menuButtonView, higlightMenuItemId);
    }

    public static void attach(
            WindowAndroid windowAndroid, AppBannerInProductHelpController controller) {
        AppBannerInProductHelpControllerProvider.attach(windowAndroid, controller);
    }

    public static void detach(AppBannerInProductHelpController controller) {
        AppBannerInProductHelpControllerProvider.detach(controller);
    }
}
