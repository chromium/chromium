// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.app.PendingIntent;
import android.content.Intent;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.Promise;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationUmaTracker;
import org.chromium.chrome.browser.notifications.NotificationWrapperBuilderFactory;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions;
import org.chromium.chrome.browser.sync.ui.PassphraseActivity;
import org.chromium.chrome.browser.sync.ui.SyncTrustedVaultProxyActivity;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.NotificationMetadata;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.browser_ui.notifications.PendingIntentProvider;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.TrustedVaultUserActionTriggerForUMA;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * {@link SyncErrorNotifier} displays Android notifications regarding sync errors.
 * Errors can be fixed by clicking the notification.
 */
public class SyncErrorNotifier implements SyncService.SyncStateChangedListener {
    @IntDef({
        NotificationState.REQUIRE_PASSPHRASE,
        NotificationState.REQUIRE_TRUSTED_VAULT_KEY_FOR_PASSWORDS,
        NotificationState.REQUIRE_TRUSTED_VAULT_KEY_FOR_EVERYTHING,
        NotificationState.HIDDEN
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface NotificationState {
        int REQUIRE_PASSPHRASE = 0;
        int REQUIRE_TRUSTED_VAULT_KEY_FOR_PASSWORDS = 1;
        int REQUIRE_TRUSTED_VAULT_KEY_FOR_EVERYTHING = 2;
        int HIDDEN = 3;
    }

    private static final String TAG = "SyncUI";

    private @Nullable static SyncErrorNotifier sInstance;
    private static boolean sInitialized;

    private final NotificationManagerProxy mNotificationManager;
    private final SyncService mSyncService;
    private final TrustedVaultClient mTrustedVaultClient;

    // What notification is being shown, if any. In truth, for REQUIRE_TRUSTED_VAULT_* states this
    // is set slightly earlier, when the class calls createTrustedVaultKeyRetrievalIntent().
    private @NotificationState int mNotificationState = NotificationState.HIDDEN;

    /** Returns null if there's no instance of SyncService (Sync disabled via command-line). */
    public static @Nullable SyncErrorNotifier get() {
        ThreadUtils.assertOnUiThread();
        if (!sInitialized) {
            if (SyncServiceFactory.get() != null) {
                sInstance =
                        new SyncErrorNotifier(
                                new NotificationManagerProxyImpl(
                                        ContextUtils.getApplicationContext()),
                                SyncServiceFactory.get(),
                                TrustedVaultClient.get());
            }
            sInitialized = true;
        }
        return sInstance;
    }

    @VisibleForTesting
    public SyncErrorNotifier(
            NotificationManagerProxy notificationManager,
            SyncService syncService,
            TrustedVaultClient trustedVaultClient) {
        mNotificationManager = notificationManager;
        mSyncService = syncService;
        mTrustedVaultClient = trustedVaultClient;
        mSyncService.addSyncStateChangedListener(this);
    }

    /**
     * {@link SyncService.SyncStateChangedListener} implementation.
     * Decides which error notification to show (if any), based on the sync state.
     */
    @Override
    public void syncStateChanged() {
        ThreadUtils.assertOnUiThread();

        final @NotificationState int goalState = computeGoalNotificationState();
        if (mNotificationState == goalState) {
            // Quite common, syncStateChanged() is triggered often. Spare NotificationManager calls
            // by early returning, they are expensive.
            // This also covers the case where the class is transitioning to REQUIRE_TRUSTED_VAULT_*
            // but createTrustedVaultKeyRetrievalIntent() hasn't responded yet. In that case this
            // check spares new createTrustedVaultKeyRetrievalIntent() calls.
            return;
        }

        @NotificationState int previousState = mNotificationState;
        mNotificationState = goalState;
        switch (goalState) {
            case NotificationState.HIDDEN:
                {
                    mNotificationManager.cancel(NotificationConstants.NOTIFICATION_ID_SYNC);
                    break;
                }
            case NotificationState.REQUIRE_PASSPHRASE:
                {
                    mSyncService.markPassphrasePromptMutedForCurrentProductVersion();
                    showNotification(
                            R.string.sync_error_card_title,
                            R.string.hint_passphrase_required,
                            createPassphraseIntent());
                    break;
                }
            case NotificationState.REQUIRE_TRUSTED_VAULT_KEY_FOR_PASSWORDS:
            case NotificationState.REQUIRE_TRUSTED_VAULT_KEY_FOR_EVERYTHING:
                {
                    createTrustedVaultKeyRetrievalIntent()
                            .then(
                                    intent -> {
                                        if (mNotificationState != goalState) {
                                            // State changed in the meantime, throw the intent away.
                                            return;
                                        }
                                        if (mNotificationState
                                                == NotificationState
                                                        .REQUIRE_TRUSTED_VAULT_KEY_FOR_PASSWORDS) {
                                            showNotification(
                                                    R.string.password_sync_error_summary,
                                                    R.string.hint_sync_retrieve_keys_for_passwords,
                                                    intent);
                                        } else {
                                            showNotification(
                                                    R.string.sync_error_card_title,
                                                    R.string.hint_sync_retrieve_keys_for_everything,
                                                    intent);
                                        }
                                    },
                                    exception -> {
                                        if (mNotificationState != goalState) {
                                            // State changed in the meantime. Lucky us, because we'd
                                            // have no intent to show the notification :).
                                            return;
                                        }
                                        // We still want to show the trusted vault notification but
                                        // couldn't produce the intent. Just reset the state.
                                        mNotificationState = previousState;
                                        Log.w(
                                                TAG,
                                                "Error creating key retrieval intent: ",
                                                exception);
                                    });
                    break;
                }
            default:
                {
                    assert false;
                    break;
                }
        }
    }

    private @NotificationState int computeGoalNotificationState() {
        if (!mSyncService.isSyncFeatureEnabled()) {
            // Error notifications are only currently shown to syncing users, even though passphrase
            // and trusted vault key errors still apply to signed-in users.
            return NotificationState.HIDDEN;
        }

        if (!mSyncService.isEngineInitialized()) {
            // The notifications expose encryption errors and those can only be detected once the
            // engine is up. In the meantime, don't show anything.
            return NotificationState.HIDDEN;
        }

        if (mSyncService.isPassphraseRequiredForPreferredDataTypes()
                && !mSyncService.isPassphrasePromptMutedForCurrentProductVersion()) {
            return NotificationState.REQUIRE_PASSPHRASE;
        }

        if (mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()) {
            return mSyncService.isEncryptEverythingEnabled()
                    ? NotificationState.REQUIRE_TRUSTED_VAULT_KEY_FOR_EVERYTHING
                    : NotificationState.REQUIRE_TRUSTED_VAULT_KEY_FOR_PASSWORDS;
        }

        return NotificationState.HIDDEN;
    }

    /** Displays the error notification with `title` and `textBody`. Replaces any existing one. */
    private void showNotification(
            @StringRes int title, @StringRes int textBody, Intent intentTriggeredOnClick) {
        // Converting |intentTriggeredOnClick| into a PendingIntent is needed because it will be
        // handed over to the Android notification manager, a foreign application.
        // FLAG_UPDATE_CURRENT ensures any cached intent extras are updated.
        PendingIntentProvider pendingIntent =
                PendingIntentProvider.getActivity(
                        ContextUtils.getApplicationContext(),
                        0,
                        intentTriggeredOnClick,
                        PendingIntent.FLAG_UPDATE_CURRENT);

        // There is no need to provide a group summary notification because NOTIFICATION_ID_SYNC
        // ensures there's only one sync notification at a time.
        NotificationWrapper notification =
                NotificationWrapperBuilderFactory.createNotificationWrapperBuilder(
                                ChromeChannelDefinitions.ChannelId.BROWSER,
                                new NotificationMetadata(
                                        NotificationUmaTracker.SystemNotificationType.SYNC,
                                        null,
                                        NotificationConstants.NOTIFICATION_ID_SYNC))
                        .setAutoCancel(true)
                        .setContentIntent(pendingIntent)
                        .setContentTitle(getString(title))
                        .setContentText(getString(textBody))
                        .setSmallIcon(R.drawable.ic_chrome)
                        .setTicker(getString(textBody))
                        .setLocalOnly(true)
                        .setGroup(NotificationConstants.GROUP_SYNC)
                        .buildWithBigTextStyle(getString(textBody));
        mNotificationManager.notify(notification);
        NotificationUmaTracker.getInstance()
                .onNotificationShown(
                        NotificationUmaTracker.SystemNotificationType.SYNC,
                        notification.getNotification());
    }

    /**
     * Creates an intent that launches an activity that requests the sync passphrase.
     *
     * @return the intent for opening the passphrase activity
     */
    private static Intent createPassphraseIntent() {
        Intent intent = new Intent(ContextUtils.getApplicationContext(), PassphraseActivity.class);
        // This activity will become the start of a new task on this history stack.
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // Clears the task stack above this activity if it already exists.
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        return intent;
    }

    /** Creates an intent that launches an activity that retrieves the trusted vault key. */
    private Promise<Intent> createTrustedVaultKeyRetrievalIntent() {
        assert mSyncService.getAccountInfo() != null;
        Promise<Intent> promise = new Promise<Intent>();
        mTrustedVaultClient
                .createKeyRetrievalIntent(mSyncService.getAccountInfo())
                // Cf. SyncTrustedVaultProxyActivity as to why use a proxy intent.
                .then(
                        realIntent ->
                                promise.fulfill(
                                        SyncTrustedVaultProxyActivity.createKeyRetrievalProxyIntent(
                                                realIntent,
                                                TrustedVaultUserActionTriggerForUMA.NOTIFICATION)),
                        exception -> promise.reject(exception));
        return promise;
    }

    private String getString(@StringRes int messageId) {
        return ContextUtils.getApplicationContext().getString(messageId);
    }
}
