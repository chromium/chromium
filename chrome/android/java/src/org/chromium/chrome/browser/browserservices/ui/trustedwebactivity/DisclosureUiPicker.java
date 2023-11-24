// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.trustedwebactivity;

import static android.app.NotificationManager.IMPORTANCE_NONE;

import static org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId.WEBAPPS;
import static org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId.WEBAPPS_QUIET;

import android.app.NotificationChannel;
import android.os.Build;

import androidx.annotation.RequiresApi;

import dagger.Lazy;

import org.chromium.base.BuildInfo;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.TwaDisclosureUi;
import org.chromium.chrome.browser.browserservices.ui.view.DisclosureInfobar;
import org.chromium.chrome.browser.browserservices.ui.view.DisclosureNotification;
import org.chromium.chrome.browser.browserservices.ui.view.DisclosureSnackbar;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;

import javax.inject.Inject;

/**
 * Determines which of the versions of the "Running in Chrome" UI is displayed to the user.
 *
 * There are three:
 * * The old Infobar. (An Infobar doesn't go away until you accept it.)
 * * The new Notification. (When notifications are enabled.)
 * * The new Snackbar. (A Snackbar dismisses automatically, this one after 7 seconds.)
 */
@ActivityScope
public class DisclosureUiPicker implements NativeInitObserver {
    private final Lazy<DisclosureInfobar> mDisclosureInfobar;
    private final Lazy<DisclosureSnackbar> mDisclosureSnackbar;
    private final Lazy<DisclosureNotification> mDisclosureNotification;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final NotificationManagerProxy mNotificationManager;

    @Inject
    public DisclosureUiPicker(
            Lazy<DisclosureInfobar> disclosureInfobar,
            Lazy<DisclosureSnackbar> disclosureSnackbar,
            Lazy<DisclosureNotification> disclosureNotification,
            BrowserServicesIntentDataProvider intentDataProvider,
            NotificationManagerProxy notificationManager,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mDisclosureInfobar = disclosureInfobar;
        mDisclosureSnackbar = disclosureSnackbar;
        mDisclosureNotification = disclosureNotification;
        mIntentDataProvider = intentDataProvider;
        mNotificationManager = notificationManager;
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
        } else if (areHeadsUpNotificationsEnabled()) {
            mDisclosureNotification.get().onStartWithNative();
        } else {
            mDisclosureSnackbar.get().showIfNeeded();
        }
    }

    private boolean areHeadsUpNotificationsEnabled() {
        if (!mNotificationManager.areNotificationsEnabled()) return false;
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return true;
        // Android Automotive doesn't currently allow heads-up notifications.
        if (BuildInfo.getInstance().isAutomotive) return false;

        return isChannelEnabled(WEBAPPS) && isChannelEnabled(WEBAPPS_QUIET);
    }

    @RequiresApi(Build.VERSION_CODES.O)
    private boolean isChannelEnabled(String channelId) {
        NotificationChannel channel = mNotificationManager.getNotificationChannel(channelId);

        // If the Channel is null we've not created it yet. Since we know that Chrome notifications
        // are not disabled in general, we know that once the channel is created it should be
        // enabled.
        if (channel == null) return true;

        return channel.getImportance() != IMPORTANCE_NONE;
    }
}
