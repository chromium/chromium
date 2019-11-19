// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.util.Log;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.ChromeNotification;
import org.chromium.chrome.browser.notifications.ChromeNotificationBuilder;
import org.chromium.chrome.browser.notifications.NotificationBuilderFactory;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationManagerProxy;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;
import org.chromium.chrome.browser.notifications.NotificationMetadata;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.PendingIntentProvider;
import org.chromium.chrome.browser.notifications.channels.ChannelDefinitions;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.sync.SyncAndServicesPreferences;
import org.chromium.chrome.browser.sync.GoogleServiceAuthError.State;
import org.chromium.chrome.browser.sync.ui.PassphraseActivity;
import org.chromium.components.sync.AndroidSyncSettings;
import org.chromium.components.sync.Passphrase;

/**
 * {@link SyncNotificationController} provides functionality for displaying Android notifications
 * regarding the user sync status.
 */
public class SyncNotificationController implements ProfileSyncService.SyncStateChangedListener {
    private static final String TAG = "SyncNotificationController";
    private final NotificationManagerProxy mNotificationManager;
    private final ProfileSyncService mProfileSyncService;

    public SyncNotificationController() {
        mNotificationManager =
                new NotificationManagerProxyImpl(ContextUtils.getApplicationContext());
        mProfileSyncService = ProfileSyncService.get();
        assert mProfileSyncService != null;
    }

    /**
     * Callback for {@link ProfileSyncService.SyncStateChangedListener}.
     */
    @Override
    public void syncStateChanged() {
        ThreadUtils.assertOnUiThread();

        // Auth errors take precedence over passphrase errors.
        if (!AndroidSyncSettings.get().isSyncEnabled()) {
            mNotificationManager.cancel(NotificationConstants.NOTIFICATION_ID_SYNC);
            return;
        }
        if (shouldSyncAuthErrorBeShown()) {
            showSyncNotification(
                    GoogleServiceAuthError.getMessageID(mProfileSyncService.getAuthError()),
                    createSettingsIntent());
        } else if (mProfileSyncService.isEngineInitialized()
                && mProfileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            if (mProfileSyncService.isPassphrasePrompted()) {
                return;
            }
            switch (mProfileSyncService.getPassphraseType()) {
                case Passphrase.Type.IMPLICIT: // Falling through intentionally.
                case Passphrase.Type.FROZEN_IMPLICIT: // Falling through intentionally.
                case Passphrase.Type.CUSTOM:
                    showSyncNotification(R.string.sync_need_passphrase, createPasswordIntent());
                    break;
                case Passphrase.Type.KEYSTORE: // Falling through intentionally.
                default:
                    mNotificationManager.cancel(NotificationConstants.NOTIFICATION_ID_SYNC);
                    return;
            }
        } else {
            mNotificationManager.cancel(NotificationConstants.NOTIFICATION_ID_SYNC);
            return;
        }
    }

    /**
     * Builds and shows a notification for the |message|.
     *
     * @param message Resource id of the message to display in the notification.
     * @param intent Intent to send when the user activates the notification.
     */
    private void showSyncNotification(int message, Intent intent) {
        Context applicationContext = ContextUtils.getApplicationContext();
        String title = null, text = null;
        // From Android N, notification by default has the app name and title should not be the same
        // as app name.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            title = applicationContext.getString(R.string.sign_in_sync);
            text = applicationContext.getString(message);
        } else {
            title = applicationContext.getString(R.string.app_name);
            text = applicationContext.getString(R.string.sign_in_sync) + ": "
                    + applicationContext.getString(message);
        }

        PendingIntentProvider contentIntent =
                PendingIntentProvider.getActivity(applicationContext, 0, intent, 0);

        // There is no need to provide a group summary notification because the NOTIFICATION_ID_SYNC
        // notification id ensures there's only one sync notification at a time.
        ChromeNotificationBuilder builder =
                NotificationBuilderFactory
                        .createChromeNotificationBuilder(true /* preferCompat */,
                                ChannelDefinitions.ChannelId.BROWSER, null /*remoteAppPackageName*/,
                                new NotificationMetadata(
                                        NotificationUmaTracker.SystemNotificationType.SYNC, null,
                                        NotificationConstants.NOTIFICATION_ID_SYNC))
                        .setAutoCancel(true)
                        .setContentIntent(contentIntent)
                        .setContentTitle(title)
                        .setContentText(text)
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setTicker(text)
                        .setLocalOnly(true)
                        .setGroup(NotificationConstants.GROUP_SYNC);

        ChromeNotification notification = builder.buildWithBigTextStyle(text);

        mNotificationManager.notify(notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                NotificationUmaTracker.SystemNotificationType.SYNC, notification.getNotification());
    }

    private boolean shouldSyncAuthErrorBeShown() {
        switch (mProfileSyncService.getAuthError()) {
            case State.NONE:
            case State.CONNECTION_FAILED:
            case State.SERVICE_UNAVAILABLE:
            case State.REQUEST_CANCELED:
            case State.INVALID_GAIA_CREDENTIALS:
                return false;
            case State.USER_NOT_SIGNED_UP:
                return true;
            default:
                Log.w(TAG, "Not showing unknown Auth Error: " + mProfileSyncService.getAuthError());
                return false;
        }
    }

    /**
     * Creates an intent that launches the Chrome settings, and automatically opens the fragment
     * for signed in users.
     *
     * @return the intent for opening the settings
     */
    private Intent createSettingsIntent() {
        return PreferencesLauncher.createIntentForSettingsPage(ContextUtils.getApplicationContext(),
                SyncAndServicesPreferences.class.getName(),
                SyncAndServicesPreferences.createArguments(false));
    }

    /**
     * Creates an intent that launches an activity that requests the users password/passphrase.
     *
     * @return the intent for opening the password/passphrase activity
     */
    private Intent createPasswordIntent() {
        // Make sure we don't prompt too many times.
        mProfileSyncService.setPassphrasePrompted(true);

        Intent intent = new Intent(ContextUtils.getApplicationContext(), PassphraseActivity.class);
        // This activity will become the start of a new task on this history stack.
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // Clears the task stack above this activity if it already exists.
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        return intent;
    }
}
