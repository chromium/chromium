// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.sync;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.os.Bundle;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.SyncService;

/** Unit tests for {@link SyncErrorNotifier}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SyncErrorNotifierTest {
    @Rule public final MockitoRule mockitoRule = MockitoJUnit.rule();

    private final Context mContext = ContextUtils.getApplicationContext();

    @Mock private NotificationManagerProxy mNotificationManagerProxy;
    @Mock private SyncService mSyncService;
    @Mock private TrustedVaultClient mTrustedVaultClient;

    @Captor private ArgumentCaptor<NotificationWrapper> mNotificationWrapperCaptor;

    @Test
    @SmallTest
    public void testNoNotification() {
        when(mSyncService.getAccountInfo()).thenReturn(null);
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(false);
        when(mSyncService.isEngineInitialized()).thenReturn(false);
        when(mSyncService.isEncryptEverythingEnabled()).thenReturn(false);
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(false);
        when(mSyncService.isPassphrasePromptMutedForCurrentProductVersion()).thenReturn(false);
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(false);

        SyncErrorNotifier notifier =
                new SyncErrorNotifier(mNotificationManagerProxy, mSyncService, mTrustedVaultClient);
        notifier.syncStateChanged();

        verify(mNotificationManagerProxy, Mockito.times(0)).notify(any());
        verify(mNotificationManagerProxy, Mockito.times(0)).cancel(anyInt());
    }

    @Test
    @SmallTest
    public void testPassphraseNotification() {
        when(mSyncService.getAccountInfo())
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId("a@b.com", "gaiaId"));
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.isEncryptEverythingEnabled()).thenReturn(true);
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(true);
        when(mSyncService.isPassphrasePromptMutedForCurrentProductVersion()).thenReturn(false);
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(false);

        SyncErrorNotifier notifier =
                new SyncErrorNotifier(mNotificationManagerProxy, mSyncService, mTrustedVaultClient);
        notifier.syncStateChanged();

        verify(mSyncService).markPassphrasePromptMutedForCurrentProductVersion();
        verify(mNotificationManagerProxy).notify(mNotificationWrapperCaptor.capture());
        Bundle notificationExtras = mNotificationWrapperCaptor.getValue().getNotification().extras;
        assertEquals(
                notificationExtras.getCharSequence(Notification.EXTRA_TITLE),
                mContext.getString(R.string.sync_error_card_title));
        assertEquals(
                notificationExtras.getCharSequence(Notification.EXTRA_TEXT),
                mContext.getString(R.string.hint_passphrase_required));

        // Spurious syncStateChanged()...
        notifier.syncStateChanged();

        // ...must cause no additional notify() calls.
        verify(mNotificationManagerProxy).notify(any());
        verify(mNotificationManagerProxy, Mockito.times(0)).cancel(anyInt());

        // Resolve the error.
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(false);
        notifier.syncStateChanged();

        // Notification must be cleared.
        verify(mNotificationManagerProxy).cancel(anyInt());
    }

    @Test
    @SmallTest
    public void testPassphraseNotificationMuted() {
        when(mSyncService.getAccountInfo())
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId("a@b.com", "gaiaId"));
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.isEncryptEverythingEnabled()).thenReturn(true);
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(true);
        when(mSyncService.isPassphrasePromptMutedForCurrentProductVersion()).thenReturn(true);
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(false);

        SyncErrorNotifier notifier =
                new SyncErrorNotifier(mNotificationManagerProxy, mSyncService, mTrustedVaultClient);
        notifier.syncStateChanged();

        verify(mNotificationManagerProxy, Mockito.times(0)).cancel(anyInt());
        verify(mNotificationManagerProxy, Mockito.times(0)).notify(any());
    }

    @Test
    @SmallTest
    public void testTrustedVaultNotificationForPasswords() {
        when(mSyncService.getAccountInfo())
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId("a@b.com", "gaiaId"));
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.isEncryptEverythingEnabled()).thenReturn(false);
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(false);
        when(mSyncService.isPassphrasePromptMutedForCurrentProductVersion()).thenReturn(false);
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(true);
        Promise<PendingIntent> intentPromise = new Promise<>();
        when(mTrustedVaultClient.createKeyRetrievalIntent(any())).thenReturn(intentPromise);

        SyncErrorNotifier notifier =
                new SyncErrorNotifier(mNotificationManagerProxy, mSyncService, mTrustedVaultClient);
        notifier.syncStateChanged();

        // Client started creating the intent but hasn't finished yet, so no notification.
        verify(mTrustedVaultClient).createKeyRetrievalIntent(any());
        verify(mNotificationManagerProxy, Mockito.times(0)).notify(any());

        notifier.syncStateChanged();

        // New calls to createKeyRetrievalIntent() must be suppressed because the first one is still
        // in flight. No notification yet.
        verify(mTrustedVaultClient).createKeyRetrievalIntent(any());
        verify(mNotificationManagerProxy, Mockito.times(0)).notify(any());

        // Return the intent (can be null as it's unused by the test).
        intentPromise.fulfill(null);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Notification must be shown now.
        verify(mNotificationManagerProxy).notify(mNotificationWrapperCaptor.capture());
        Bundle notificationExtras = mNotificationWrapperCaptor.getValue().getNotification().extras;
        assertEquals(
                notificationExtras.getCharSequence(Notification.EXTRA_TITLE),
                mContext.getString(R.string.password_sync_error_summary));
        assertEquals(
                notificationExtras.getCharSequence(Notification.EXTRA_TEXT),
                mContext.getString(R.string.hint_sync_retrieve_keys_for_passwords));

        // Spurious syncStateChanged()...
        notifier.syncStateChanged();

        // ...must be a no-op, i.e. no additional notify() / createKeyRetrievalIntent() calls.
        verify(mNotificationManagerProxy).notify(any());
        verify(mTrustedVaultClient).createKeyRetrievalIntent(any());
        verify(mNotificationManagerProxy, Mockito.times(0)).cancel(anyInt());

        // Resolve the error.
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(false);
        notifier.syncStateChanged();

        // Notification must be cleared.
        verify(mNotificationManagerProxy).cancel(anyInt());
    }

    @Test
    @SmallTest
    public void testTrustedVaultNotificationForEverything() {
        when(mSyncService.getAccountInfo())
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId("a@b.com", "gaiaId"));
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.isEncryptEverythingEnabled()).thenReturn(true);
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(false);
        when(mSyncService.isPassphrasePromptMutedForCurrentProductVersion()).thenReturn(false);
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(true);
        when(mTrustedVaultClient.createKeyRetrievalIntent(any()))
                .thenReturn(Promise.fulfilled(null));

        SyncErrorNotifier notifier =
                new SyncErrorNotifier(mNotificationManagerProxy, mSyncService, mTrustedVaultClient);
        notifier.syncStateChanged();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Strings must be different from testTrustedVaultNotificationForPasswords()
        verify(mNotificationManagerProxy, Mockito.times(0)).cancel(anyInt());
        verify(mNotificationManagerProxy).notify(mNotificationWrapperCaptor.capture());
        Bundle notificationExtras = mNotificationWrapperCaptor.getValue().getNotification().extras;
        assertEquals(
                notificationExtras.getCharSequence(Notification.EXTRA_TITLE),
                mContext.getString(R.string.sync_error_card_title));
        assertEquals(
                notificationExtras.getCharSequence(Notification.EXTRA_TEXT),
                mContext.getString(R.string.hint_sync_retrieve_keys_for_everything));
    }

    @Test
    @SmallTest
    public void testTrustedVaultIntentCreationFails() {
        when(mSyncService.getAccountInfo())
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId("a@b.com", "gaiaId"));
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.isEncryptEverythingEnabled()).thenReturn(true);
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(false);
        when(mSyncService.isPassphrasePromptMutedForCurrentProductVersion()).thenReturn(false);
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(true);
        when(mTrustedVaultClient.createKeyRetrievalIntent(any())).thenReturn(Promise.rejected());

        SyncErrorNotifier notifier =
                new SyncErrorNotifier(mNotificationManagerProxy, mSyncService, mTrustedVaultClient);
        notifier.syncStateChanged();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // There must've been no notify() calls - because the intent creation failed - and no
        // cancel() calls either - because there were no ongoing notifications to cancel.
        verify(mNotificationManagerProxy, Mockito.times(0)).notify(any());
        verify(mNotificationManagerProxy, Mockito.times(0)).cancel(anyInt());
    }

    @Test
    @SmallTest
    public void testPassphraseNotificationForSignedInUsers() {
        when(mSyncService.getAccountInfo())
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId("a@b.com", "gaiaId"));
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(false);
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.isEncryptEverythingEnabled()).thenReturn(true);
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(true);
        when(mSyncService.isPassphrasePromptMutedForCurrentProductVersion()).thenReturn(false);
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(false);

        SyncErrorNotifier notifier =
                new SyncErrorNotifier(mNotificationManagerProxy, mSyncService, mTrustedVaultClient);
        notifier.syncStateChanged();

        verify(mSyncService).markPassphrasePromptMutedForCurrentProductVersion();
        verify(mNotificationManagerProxy).notify(mNotificationWrapperCaptor.capture());
        Bundle notificationExtras = mNotificationWrapperCaptor.getValue().getNotification().extras;
        assertEquals(
                notificationExtras.getCharSequence(Notification.EXTRA_TITLE),
                mContext.getString(R.string.identity_error_message_title_passphrase_required));
        assertEquals(
                notificationExtras.getCharSequence(Notification.EXTRA_TEXT),
                mContext.getString(R.string.identity_error_message_body));

        // Spurious syncStateChanged()...
        notifier.syncStateChanged();

        // ...must cause no additional notify() calls.
        verify(mNotificationManagerProxy).notify(any());
        verify(mNotificationManagerProxy, Mockito.times(0)).cancel(anyInt());

        // Resolve the error.
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(false);
        notifier.syncStateChanged();

        // Notification must be cleared.
        verify(mNotificationManagerProxy).cancel(anyInt());
    }

    @Test
    @SmallTest
    public void testTrustedVaultNotificationForPasswordsForSignedInUsers() {
        when(mSyncService.getAccountInfo())
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId("a@b.com", "gaiaId"));
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(false);
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.isEncryptEverythingEnabled()).thenReturn(false);
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(false);
        when(mSyncService.isPassphrasePromptMutedForCurrentProductVersion()).thenReturn(false);
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(true);
        Promise<PendingIntent> intentPromise = new Promise<>();
        when(mTrustedVaultClient.createKeyRetrievalIntent(any())).thenReturn(intentPromise);

        SyncErrorNotifier notifier =
                new SyncErrorNotifier(mNotificationManagerProxy, mSyncService, mTrustedVaultClient);
        notifier.syncStateChanged();

        // Client started creating the intent but hasn't finished yet, so no notification.
        verify(mTrustedVaultClient).createKeyRetrievalIntent(any());
        verify(mNotificationManagerProxy, Mockito.times(0)).notify(any());

        notifier.syncStateChanged();

        // New calls to createKeyRetrievalIntent() must be suppressed because the first one is still
        // in flight. No notification yet.
        verify(mTrustedVaultClient).createKeyRetrievalIntent(any());
        verify(mNotificationManagerProxy, Mockito.times(0)).notify(any());

        // Return the intent (can be null as it's unused by the test).
        intentPromise.fulfill(null);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Notification must be shown now.
        verify(mNotificationManagerProxy).notify(mNotificationWrapperCaptor.capture());
        Bundle notificationExtras = mNotificationWrapperCaptor.getValue().getNotification().extras;
        assertEquals(
                notificationExtras.getCharSequence(Notification.EXTRA_TITLE),
                mContext.getString(R.string.identity_error_card_button_verify));
        assertEquals(
                notificationExtras.getCharSequence(Notification.EXTRA_TEXT),
                mContext.getString(
                        R.string.identity_error_message_body_sync_retrieve_keys_for_passwords));

        // Spurious syncStateChanged()...
        notifier.syncStateChanged();

        // ...must be a no-op, i.e. no additional notify() / createKeyRetrievalIntent() calls.
        verify(mNotificationManagerProxy).notify(any());
        verify(mTrustedVaultClient).createKeyRetrievalIntent(any());
        verify(mNotificationManagerProxy, Mockito.times(0)).cancel(anyInt());

        // Resolve the error.
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(false);
        notifier.syncStateChanged();

        // Notification must be cleared.
        verify(mNotificationManagerProxy).cancel(anyInt());
    }

    @Test
    @SmallTest
    public void testTrustedVaultNotificationForEverythingForSignedInUsers() {
        when(mSyncService.getAccountInfo())
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId("a@b.com", "gaiaId"));
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(false);
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.isEncryptEverythingEnabled()).thenReturn(true);
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(false);
        when(mSyncService.isPassphrasePromptMutedForCurrentProductVersion()).thenReturn(false);
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(true);
        when(mTrustedVaultClient.createKeyRetrievalIntent(any()))
                .thenReturn(Promise.fulfilled(null));

        SyncErrorNotifier notifier =
                new SyncErrorNotifier(mNotificationManagerProxy, mSyncService, mTrustedVaultClient);
        notifier.syncStateChanged();
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // Strings must be different from testTrustedVaultNotificationForPasswords().
        verify(mNotificationManagerProxy, Mockito.times(0)).cancel(anyInt());
        verify(mNotificationManagerProxy).notify(mNotificationWrapperCaptor.capture());
        Bundle notificationExtras = mNotificationWrapperCaptor.getValue().getNotification().extras;
        assertEquals(
                notificationExtras.getCharSequence(Notification.EXTRA_TITLE),
                mContext.getString(R.string.identity_error_card_button_verify));
        assertEquals(
                notificationExtras.getCharSequence(Notification.EXTRA_TEXT),
                mContext.getString(R.string.identity_error_message_body));
    }
}
