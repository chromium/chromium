// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

import androidx.annotation.StringRes;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncher;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.settings.SyncAndServicesSettings;
import org.chromium.chrome.browser.sync.settings.SyncSettingsUtils;
import org.chromium.chrome.browser.sync.ui.PassphraseActivity;
import org.chromium.chrome.browser.sync.ui.TrustedVaultKeyRetrievalProxyActivity;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.NotificationWrapperBuilder;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GoogleServiceAuthError.State;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.PassphraseType;

/**
 * {@link SyncNotificationController} provides functionality for displaying Android notifications
 * regarding the user sync status.
 */
public class SyncNotificationController implements ProfileSyncService.SyncStateChangedListener {
    private static final String TAG = "SyncUI";
    private final NotificationManagerProxy mNotificationManager;
    private final ProfileSyncService mProfileSyncService;
    private boolean mTrustedVaultNotificationShownOrCreating;

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
        if (!mProfileSyncService.isSyncRequested()) {
            cancelNotifications();
            return;
        }
        if (shouldSyncAuthErrorBeShown()) {
            showSyncNotification(SyncSettingsUtils.getMessageID(mProfileSyncService.getAuthError()),
                    createSettingsIntent());
        } else if (mProfileSyncService.isEngineInitialized()
                && mProfileSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            assert (!mProfileSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes());

            if (mProfileSyncService.isPassphrasePrompted()) {
                return;
            }
            switch (mProfileSyncService.getPassphraseType()) {
                case PassphraseType.IMPLICIT_PASSPHRASE: // Falling through intentionally.
                case PassphraseType.FROZEN_IMPLICIT_PASSPHRASE: // Falling through intentionally.
                case PassphraseType.CUSTOM_PASSPHRASE:
                    showSyncNotification(R.string.sync_need_passphrase, createPasswordIntent());
                    break;
                case PassphraseType.TRUSTED_VAULT_PASSPHRASE:
                    assert false : "Passphrase cannot be required with trusted vault passphrase";
                    return;
                case PassphraseType.KEYSTORE_PASSPHRASE: // Falling through intentionally.
                default:
                    cancelNotifications();
                    return;
            }
        } else if (mProfileSyncService.isEngineInitialized()
                && mProfileSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()) {
            maybeCreateKeyRetrievalNotification();
        } else {
            cancelNotifications();
        }
    }

    /**
     * Cancels existing notification if there is any.
     */
    private void cancelNotifications() {
        mNotificationManager.cancel(NotificationConstants.NOTIFICATION_ID_SYNC);
        mTrustedVaultNotificationShownOrCreating = false;
    }

    /**
     * Builds and shows a notification for the |message|.
     *
     * @param message Resource id of the message to display in the notification.
     * @param contentIntent represents intent to send when the user activates the notification.
     */
    private void showSyncNotificationForPendingIntent(
            @StringRes int message, PendingIntentProvider contentIntent) {
        Context applicationContext = ContextUtils.getApplicationContext();
        String title = null;
        String text = null;
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

        // There is no need to provide a group summary notification because the NOTIFICATION_ID_SYNC
        // notification id ensures there's only one sync notification at a time.
        NotificationWrapperBuilder builder =
                NotificationWrapperBuilderFactory
                        .createNotificationWrapperBuilder(true /* preferCompat */,
                                ChromeChannelDefinitions.ChannelId.BROWSER,
                                null /*remoteAppPackageName*/,
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

        NotificationWrapper notification = builder.buildWithBigTextStyle(text);

        mNotificationManager.notify(notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                NotificationUmaTracker.SystemNotificationType.SYNC, notification.getNotification());
    }

    /**
     * Builds and shows a notification for the |message|.
     *
     * @param message Resource id of the message to display in the notification.
     * @param intent Intent to send when the user activates the notification.
     */
    private void showSyncNotification(@StringRes int message, Intent intent) {
        Context applicationContext = ContextUtils.getApplicationContext();
        // There might be cached PendingIntent for sync notification and this PendingIntent might
        // be outdated. FLAG_UPDATE_CURRENT updates cached PendingIntent.
        PendingIntentProvider contentIntent = PendingIntentProvider.getActivity(
                applicationContext, 0, intent, PendingIntent.FLAG_UPDATE_CURRENT);
        showSyncNotificationForPendingIntent(message, contentIntent);
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
        SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
        return settingsLauncher.createSettingsActivityIntent(ContextUtils.getApplicationContext(),
                SyncAndServicesSettings.class.getName(),
                SyncAndServicesSettings.createArguments(false));
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

    /**
     * Attempts to asynchronously create and show key retrieval notification if no one is already
     * created or creating and there is a primary account with SYNC ConsentLevel.
     */
    private void maybeCreateKeyRetrievalNotification() {
        CoreAccountInfo primaryAccountInfo =
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SYNC);
        // Check/set |mTrustedVaultNotificationShownOrCreating| here to ensure notification is not
        // shown again immediately after cancelling (Sync state might be changed often) and there
        // is only one asynchronous createKeyRetrievalIntent() attempt at the time triggered by
        // this function.
        // TODO(crbug.com/1071377): if the user dismissed the notification, it will reappear only
        // after browser restart or disable-enable Sync action. This is sub-optimal behavior and
        // it's better to find a way to show it more often, but not on each Sync state change.
        if (primaryAccountInfo == null || mTrustedVaultNotificationShownOrCreating) {
            return;
        }
        mTrustedVaultNotificationShownOrCreating = true;
        TrustedVaultClient.get()
                .createKeyRetrievalIntent(primaryAccountInfo)
                .then(
                        (pendingIntent)
                                -> {
                            // TODO(crbug.com/1071377): Sync state might be changed already, so
                            // this notification won't make sense.
                            showSyncNotification(mProfileSyncService.isEncryptEverythingEnabled()
                                            ? R.string.sync_error_card_title
                                            : R.string.sync_passwords_error_card_title,
                                    TrustedVaultKeyRetrievalProxyActivity
                                            .createKeyRetrievalProxyIntent(pendingIntent));
                        },
                        (exception) -> {
                            Log.w(TAG, "Error creating key retrieval intent: ", exception);
                        });
    }
}
