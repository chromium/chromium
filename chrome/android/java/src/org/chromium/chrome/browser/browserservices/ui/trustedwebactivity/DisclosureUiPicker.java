// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.trustedwebactivity;

import static android.app.NotificationManager.IMPORTANCE_NONE;

import static org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId.WEBAPPS;
import static org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId.WEBAPPS_QUIET;

import android.app.NotificationChannel;
import android.os.Build;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.TwaDisclosureUi;
import org.chromium.chrome.browser.browserservices.ui.view.DisclosureInfobar;
import org.chromium.chrome.browser.browserservices.ui.view.DisclosureNotification;
import org.chromium.chrome.browser.browserservices.ui.view.DisclosureSnackbar;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.NotificationProxyUtils;

/**
 * Determines which of the versions of the "Running in Chrome" UI is displayed to the user.
 *
 * <p>There are three: <br>
 * * The old Infobar. (An Infobar doesn't go away until you accept it.) <br>
 * * The new Notification. (When notifications are enabled.) <br>
 * * The new Snackbar. (A Snackbar dismisses automatically, this one after 7 seconds.)
 */
public class DisclosureUiPicker implements NativeInitObserver {
    private final Supplier<DisclosureInfobar> mDisclosureInfobar;
    private final Supplier<DisclosureSnackbar> mDisclosureSnackbar;
    private final Supplier<DisclosureNotification> mDisclosureNotification;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final BaseNotificationManagerProxy mNotificationManagerProxy =
            BaseNotificationManagerProxyFactory.create();

    public DisclosureUiPicker(
            Supplier<DisclosureInfobar> disclosureInfobar,
            Supplier<DisclosureSnackbar> disclosureSnackbar,
            Supplier<DisclosureNotification> disclosureNotification,
            BrowserServicesIntentDataProvider intentDataProvider,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mDisclosureInfobar = disclosureInfobar;
        mDisclosureSnackbar = disclosureSnackbar;
        mDisclosureNotification = disclosureNotification;
        mIntentDataProvider = intentDataProvider;
        lifecycleDispatcher.register(this);
    }

    @Override
    public void onFinishNativeInitialization() {
        // Calling get on the appropriate Lazy instance will cause Dagger to create the class.
        // The classes wire themselves up to the rest of the code in their constructors.

        // TODO(peconn): Once this feature is enabled by default and we get rid of this check, move
        // this logic into the constructor and let the Views call showIfNeeded() themselves in
        // their onFinishNativeInitialization.

        if (mIntentDataProvider.getTwaDisclosureUi() == TwaDisclosureUi.V1_INFOBAR) {
            mDisclosureInfobar.get().showIfNeeded();
        } else {
            areHeadsUpNotificationsEnabled(
                    (enabled) -> {
                        if (enabled) {
                            mDisclosureNotification.get().onStartWithNative();
                        } else {
                            mDisclosureSnackbar.get().showIfNeeded();
                        }
                    });
        }
    }

    private void areHeadsUpNotificationsEnabled(Callback<Boolean> callback) {
        if (!NotificationProxyUtils.areNotificationsEnabled()) {
            callback.onResult(false);
            return;
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            callback.onResult(true);
            return;
        }
        // Android Automotive doesn't currently allow heads-up notifications.
        if (BuildInfo.getInstance().isAutomotive) {
            callback.onResult(false);
            return;
        }

        mNotificationManagerProxy.getNotificationChannels(
                (channels) -> {
                    boolean isWebAppsEnabled = true;
                    boolean isWebAppsQuietEnabled = true;
                    for (NotificationChannel channel : channels) {
                        if (WEBAPPS.equals(channel.getId())) {
                            isWebAppsEnabled = (channel.getImportance() != IMPORTANCE_NONE);
                        } else if (WEBAPPS_QUIET.equals(channel.getId())) {
                            isWebAppsQuietEnabled = (channel.getImportance() != IMPORTANCE_NONE);
                        }
                    }
                    callback.onResult(isWebAppsEnabled && isWebAppsQuietEnabled);
                });
    }
}
