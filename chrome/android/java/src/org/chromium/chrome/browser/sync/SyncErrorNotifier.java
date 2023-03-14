// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.app.PendingIntent;
import android.content.Intent;

import androidx.annotation.Nullable;
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
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.ui.PassphraseActivity;
import org.chromium.chrome.browser.sync.ui.SyncTrustedVaultProxyActivity;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.PassphraseType;
import org.chromium.components.sync.TrustedVaultUserActionTriggerForUMA;

/**
 * {@link SyncErrorNotifier} displays Android notifications regarding sync errors.
 * Errors can be fixed by clicking the notification.
 */
public class SyncErrorNotifier implements SyncService.SyncStateChangedListener {
    private static final String TAG = "SyncUI";
    private final NotificationManagerProxy mNotificationManager;
    private final SyncService mSyncService;
    private boolean mTrustedVaultNotificationShownOrCreating;

    private @Nullable static SyncErrorNotifier sInstance;
    private static boolean sInitialized;

    /**
     * Returns null if there's no instance of SyncService (Sync disabled via command-line).
     */
    @Nullable
    public static SyncErrorNotifier get() {
        ThreadUtils.assertOnUiThread();
        if (!sInitialized) {
            if (SyncService.get() != null) {
                sInstance = new SyncErrorNotifier();
            }
            sInitialized = true;
        }
        return sInstance;
    }

    private SyncErrorNotifier() {
        mNotificationManager =
                new NotificationManagerProxyImpl(ContextUtils.getApplicationContext());
        mSyncService = SyncService.get();
        assert mSyncService != null;
        mSyncService.addSyncStateChangedListener(this);
    }

    /**
     * {@link SyncService.SyncStateChangedListener} implementation.
     * Decides which error notification to show (if any), based on the sync state.
     */
    @Override
    public void syncStateChanged() {
        ThreadUtils.assertOnUiThread();

        if (!mSyncService.isSyncFeatureEnabled()) {
            cancelNotifications();
        } else if (mSyncService.isEngineInitialized()
                && mSyncService.isPassphraseRequiredForPreferredDataTypes()) {
            assert (!mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes());

            if (mSyncService.isPassphrasePromptMutedForCurrentProductVersion()) {
                return;
            }

            switch (mSyncService.getPassphraseType()) {
                case PassphraseType.IMPLICIT_PASSPHRASE:
                case PassphraseType.FROZEN_IMPLICIT_PASSPHRASE:
                case PassphraseType.CUSTOM_PASSPHRASE:
                    showNotification(getString(R.string.sync_error_card_title),
                            getString(R.string.hint_passphrase_required), createPassphraseIntent());
                    break;
                case PassphraseType.TRUSTED_VAULT_PASSPHRASE:
                    assert false : "Passphrase cannot be required with trusted vault passphrase";
                    break;
                case PassphraseType.KEYSTORE_PASSPHRASE:
                    cancelNotifications();
                    break;
                default:
                    assert false : "Unknown passphrase type";
                    break;
            }
        } else if (mSyncService.isEngineInitialized()
                && mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()) {
            maybeShowKeyRetrievalNotification();
        } else {
            cancelNotifications();
        }
    }

    private void cancelNotifications() {
        mNotificationManager.cancel(NotificationConstants.NOTIFICATION_ID_SYNC);
        mTrustedVaultNotificationShownOrCreating = false;
    }

    /**
     * Displays the error notification with content |textBody|. The title of the notification is
     * fixed.
     */
    private void showNotification(String title, String textBody, Intent intentTriggeredOnClick) {
        // Converting |intentTriggeredOnClick| into a PendingIntent is needed because it will be
        // handed over to the Android notification manager, a foreign application.
        // FLAG_UPDATE_CURRENT ensures any cached intent extras are updated.
        PendingIntentProvider pendingIntent =
                PendingIntentProvider.getActivity(ContextUtils.getApplicationContext(), 0,
                        intentTriggeredOnClick, PendingIntent.FLAG_UPDATE_CURRENT);

        // There is no need to provide a group summary notification because NOTIFICATION_ID_SYNC
        // ensures there's only one sync notification at a time.
        NotificationWrapper notification =
                NotificationWrapperBuilderFactory
                        .createNotificationWrapperBuilder(
                                ChromeChannelDefinitions.ChannelId.BROWSER,
                                new NotificationMetadata(
                                        NotificationUmaTracker.SystemNotificationType.SYNC, null,
                                        NotificationConstants.NOTIFICATION_ID_SYNC))
                        .setAutoCancel(true)
                        .setContentIntent(pendingIntent)
                        .setContentTitle(title)
                        .setContentText(textBody)
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setTicker(textBody)
                        .setLocalOnly(true)
                        .setGroup(NotificationConstants.GROUP_SYNC)
                        .buildWithBigTextStyle(textBody);
        mNotificationManager.notify(notification);
        NotificationUmaTracker.getInstance().onNotificationShown(
                NotificationUmaTracker.SystemNotificationType.SYNC, notification.getNotification());
    }

    /**
     * Creates an intent that launches an activity that requests the sync passphrase.
     *
     * @return the intent for opening the passphrase activity
     */
    private Intent createPassphraseIntent() {
        // Make sure we don't prompt too many times.
        mSyncService.markPassphrasePromptMutedForCurrentProductVersion();

        Intent intent = new Intent(ContextUtils.getApplicationContext(), PassphraseActivity.class);
        // This activity will become the start of a new task on this history stack.
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // Clears the task stack above this activity if it already exists.
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        return intent;
    }

    /**
     * Attempts to asynchronously show a key retrieval notification if a) one doesn't
     * already exist or is being created; and b) there is a primary account with ConsentLevel.SYNC.
     */
    private void maybeShowKeyRetrievalNotification() {
        CoreAccountInfo primaryAccountInfo =
                IdentityServicesProvider.get()
                        .getIdentityManager(Profile.getLastUsedRegularProfile())
                        .getPrimaryAccountInfo(ConsentLevel.SYNC);
        // Check/set |mTrustedVaultNotificationShownOrCreating| here to ensure the notification is
        // not shown again immediately after cancelling (Sync state might be changed often) and
        // there is only one asynchronous createKeyRetrievalIntent() attempt at a time.
        if (primaryAccountInfo == null || mTrustedVaultNotificationShownOrCreating) {
            return;
        }
        mTrustedVaultNotificationShownOrCreating = true;

        String notificationTitle = getString(mSyncService.isEncryptEverythingEnabled()
                        ? R.string.sync_error_card_title
                        : R.string.password_sync_error_summary);
        String notificationTextBody = getString(mSyncService.isEncryptEverythingEnabled()
                        ? R.string.hint_sync_retrieve_keys_for_everything
                        : R.string.hint_sync_retrieve_keys_for_passwords);

        TrustedVaultClient.get()
                .createKeyRetrievalIntent(primaryAccountInfo)
                // Cf. SyncTrustedVaultProxyActivity as to why use a proxy intent.
                .then((realIntent)
                                -> showNotification(notificationTitle, notificationTextBody,
                                        SyncTrustedVaultProxyActivity.createKeyRetrievalProxyIntent(
                                                realIntent,
                                                TrustedVaultUserActionTriggerForUMA.NOTIFICATION)),
                        (exception)
                                -> Log.w(TAG, "Error creating key retrieval intent: ", exception));
    }

    private String getString(@StringRes int messageId) {
        return ContextUtils.getApplicationContext().getString(messageId);
    }
}
