// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.sync;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Notification;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.notifications.NotificationConstants;
import org.chromium.chrome.browser.notifications.NotificationIntentInterceptor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileResolver;
import org.chromium.chrome.browser.profiles.ProfileResolverJni;
import org.chromium.chrome.browser.profiles.ProfileTokenUtils;
import org.chromium.components.browser_ui.notifications.NotificationFeatureMap;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationWrapper;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.sync.SyncService;
import org.chromium.components.trusted_vault.TrustedVaultClient;
import org.chromium.google_apis.gaia.GaiaId;

import java.util.List;

/** Unit tests for {@link SyncErrorNotifier}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({NotificationFeatureMap.CACHE_NOTIIFICATIONS_ENABLED})
public class SyncErrorNotifierTest {
    private static final String LEGACY_NOTIFICATION_TAG = "SyncUI";

    @Rule public final MockitoRule mockitoRule = MockitoJUnit.rule();

    private final Context mContext = ContextUtils.getApplicationContext();

    @Mock private NotificationManagerProxy mNotificationManagerProxy;
    @Mock private SyncService mSyncService;
    @Mock private TrustedVaultClient mTrustedVaultClient;
    @Mock private TrustedVaultClient.ProxyIntentFactory mTrustedVaultProxyIntent;
    @Mock private Profile mProfile;
    @Mock private ProfileResolver.Natives mProfileResolverNatives;

    @Captor private ArgumentCaptor<NotificationWrapper> mNotificationWrapperCaptor;

    private String mNotificationTag;

    @Before
    public void setup() {
        ProfileResolverJni.setInstanceForTesting(mProfileResolverNatives);
        when(mProfileResolverNatives.tokenizeProfileToBytes(mProfile))
                .thenReturn(new byte[] {1, 2, 3});
        mNotificationTag = ProfileTokenUtils.getNotificationTag("SyncUI", mProfile);
        when(mTrustedVaultClient.getProxyIntentFactory()).thenReturn(mTrustedVaultProxyIntent);
        when(mTrustedVaultProxyIntent.createKeyRetrievalProxyIntent(any(), anyInt(), anyInt()))
                .thenAnswer(invocation -> new Intent().setPackage(mContext.getPackageName()));
    }

    @Test
    public void testNoNotification() {
        when(mSyncService.getAccountInfo()).thenReturn(null);
        when(mSyncService.isEngineInitialized()).thenReturn(false);
        when(mSyncService.isEncryptEverythingEnabled()).thenReturn(false);
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(false);
        when(mSyncService.isPassphrasePromptMutedForCurrentProductVersion()).thenReturn(false);
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(false);

        SyncErrorNotifier notifier =
                new SyncErrorNotifier(
                        mNotificationManagerProxy, mSyncService, mTrustedVaultClient, mProfile);
        // The notification posted before notifications were per-Profile is cleared on
        // construction, even without a sync error.
        verify(mNotificationManagerProxy)
                .cancel(
                        eq(LEGACY_NOTIFICATION_TAG),
                        eq(NotificationConstants.NOTIFICATION_ID_SYNC));
        notifier.syncStateChanged();

        verify(mNotificationManagerProxy, Mockito.times(0)).notify(any());
        verify(mNotificationManagerProxy, Mockito.times(0)).cancel(eq(mNotificationTag), anyInt());
        verify(mNotificationManagerProxy).cancel(eq(LEGACY_NOTIFICATION_TAG), anyInt());
    }

    @Test
    public void testPassphraseNotificationMuted() {
        when(mSyncService.getAccountInfo())
                .thenReturn(
                        CoreAccountInfo.createFromEmailAndGaiaId("a@b.com", new GaiaId("gaiaId")));
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.isEncryptEverythingEnabled()).thenReturn(true);
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(true);
        when(mSyncService.isPassphrasePromptMutedForCurrentProductVersion()).thenReturn(true);
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(false);

        SyncErrorNotifier notifier =
                new SyncErrorNotifier(
                        mNotificationManagerProxy, mSyncService, mTrustedVaultClient, mProfile);
        notifier.syncStateChanged();

        verify(mNotificationManagerProxy, Mockito.times(0)).cancel(eq(mNotificationTag), anyInt());
        verify(mNotificationManagerProxy).cancel(eq(LEGACY_NOTIFICATION_TAG), anyInt());
        verify(mNotificationManagerProxy, Mockito.times(0)).notify(any());
    }

    @Test
    public void testTrustedVaultIntentCreationFails() {
        when(mSyncService.getAccountInfo())
                .thenReturn(
                        CoreAccountInfo.createFromEmailAndGaiaId("a@b.com", new GaiaId("gaiaId")));
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.isEncryptEverythingEnabled()).thenReturn(true);
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(false);
        when(mSyncService.isPassphrasePromptMutedForCurrentProductVersion()).thenReturn(false);
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(true);
        when(mTrustedVaultClient.createKeyRetrievalIntent(any())).thenReturn(Promise.rejected());

        SyncErrorNotifier notifier =
                new SyncErrorNotifier(
                        mNotificationManagerProxy, mSyncService, mTrustedVaultClient, mProfile);
        notifier.syncStateChanged();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        // There must've been no notify() calls - because the intent creation failed - and no
        // cancel() calls for this Profile's tag either - because there were no ongoing
        // notifications to cancel.
        verify(mNotificationManagerProxy, Mockito.times(0)).notify(any());
        verify(mNotificationManagerProxy, Mockito.times(0)).cancel(eq(mNotificationTag), anyInt());
        verify(mNotificationManagerProxy).cancel(eq(LEGACY_NOTIFICATION_TAG), anyInt());
    }

    @Test
    public void testPassphraseNotificationForSignedInUsers() {
        when(mSyncService.getAccountInfo())
                .thenReturn(
                        CoreAccountInfo.createFromEmailAndGaiaId("a@b.com", new GaiaId("gaiaId")));
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.isEncryptEverythingEnabled()).thenReturn(true);
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(true);
        when(mSyncService.isPassphrasePromptMutedForCurrentProductVersion()).thenReturn(false);
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(false);

        SyncErrorNotifier notifier =
                new SyncErrorNotifier(
                        mNotificationManagerProxy, mSyncService, mTrustedVaultClient, mProfile);
        notifier.syncStateChanged();

        verify(mSyncService).markPassphrasePromptMutedForCurrentProductVersion();
        verify(mNotificationManagerProxy).notify(mNotificationWrapperCaptor.capture());
        assertEquals(mNotificationTag, mNotificationWrapperCaptor.getValue().getMetadata().tag);
        Intent contentIntent = getContentIntent(mNotificationWrapperCaptor.getValue());
        assertTrue(ProfileTokenUtils.hasProfileToken(contentIntent));
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
        verify(mNotificationManagerProxy, Mockito.times(0)).cancel(eq(mNotificationTag), anyInt());

        // Resolve the error.
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(false);
        notifier.syncStateChanged();

        // Notification must be cleared.
        verify(mNotificationManagerProxy).cancel(eq(mNotificationTag), anyInt());
        // The notification posted before notifications were per-Profile is only cleared once, on
        // construction, not again when showing or clearing the error.
        verify(mNotificationManagerProxy).cancel(eq(LEGACY_NOTIFICATION_TAG), anyInt());
    }

    @Test
    public void testTrustedVaultNotificationForPasswordsForSignedInUsers() {
        when(mSyncService.getAccountInfo())
                .thenReturn(
                        CoreAccountInfo.createFromEmailAndGaiaId("a@b.com", new GaiaId("gaiaId")));
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.isEncryptEverythingEnabled()).thenReturn(false);
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(false);
        when(mSyncService.isPassphrasePromptMutedForCurrentProductVersion()).thenReturn(false);
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(true);
        Promise<PendingIntent> intentPromise = new Promise<>();
        when(mTrustedVaultClient.createKeyRetrievalIntent(any())).thenReturn(intentPromise);

        SyncErrorNotifier notifier =
                new SyncErrorNotifier(
                        mNotificationManagerProxy, mSyncService, mTrustedVaultClient, mProfile);
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
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

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
        verify(mNotificationManagerProxy, Mockito.times(0)).cancel(eq(mNotificationTag), anyInt());

        // Resolve the error.
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(false);
        notifier.syncStateChanged();

        // Notification must be cleared.
        verify(mNotificationManagerProxy).cancel(eq(mNotificationTag), anyInt());
    }

    @Test
    public void testTrustedVaultNotificationForEverythingForSignedInUsers() {
        when(mSyncService.getAccountInfo())
                .thenReturn(
                        CoreAccountInfo.createFromEmailAndGaiaId("a@b.com", new GaiaId("gaiaId")));
        when(mSyncService.isEngineInitialized()).thenReturn(true);
        when(mSyncService.isEncryptEverythingEnabled()).thenReturn(true);
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(false);
        when(mSyncService.isPassphrasePromptMutedForCurrentProductVersion()).thenReturn(false);
        when(mSyncService.isTrustedVaultKeyRequiredForPreferredDataTypes()).thenReturn(true);
        when(mTrustedVaultClient.createKeyRetrievalIntent(any()))
                .thenReturn(Promise.fulfilled(null));

        SyncErrorNotifier notifier =
                new SyncErrorNotifier(
                        mNotificationManagerProxy, mSyncService, mTrustedVaultClient, mProfile);
        notifier.syncStateChanged();
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        // Strings must be different from testTrustedVaultNotificationForPasswords().
        verify(mNotificationManagerProxy, Mockito.times(0)).cancel(eq(mNotificationTag), anyInt());
        verify(mNotificationManagerProxy).notify(mNotificationWrapperCaptor.capture());
        Bundle notificationExtras = mNotificationWrapperCaptor.getValue().getNotification().extras;
        assertEquals(
                notificationExtras.getCharSequence(Notification.EXTRA_TITLE),
                mContext.getString(R.string.identity_error_card_button_verify));
        assertEquals(
                notificationExtras.getCharSequence(Notification.EXTRA_TEXT),
                mContext.getString(R.string.identity_error_message_body));
    }

    @Test
    public void testNotificationsArePerProfile() {
        Profile otherProfile = Mockito.mock(Profile.class);
        SyncService otherSyncService = Mockito.mock(SyncService.class);
        when(mProfileResolverNatives.tokenizeProfileToBytes(otherProfile))
                .thenReturn(new byte[] {4, 5, 6});
        String otherNotificationTag = ProfileTokenUtils.getNotificationTag("SyncUI", otherProfile);
        assertNotEquals(mNotificationTag, otherNotificationTag);
        for (SyncService syncService : List.of(mSyncService, otherSyncService)) {
            when(syncService.getAccountInfo())
                    .thenReturn(
                            CoreAccountInfo.createFromEmailAndGaiaId(
                                    "a@b.com", new GaiaId("gaiaId")));
            when(syncService.isEngineInitialized()).thenReturn(true);
            when(syncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(true);
        }

        SyncErrorNotifier notifier =
                new SyncErrorNotifier(
                        mNotificationManagerProxy, mSyncService, mTrustedVaultClient, mProfile);
        SyncErrorNotifier otherNotifier =
                new SyncErrorNotifier(
                        mNotificationManagerProxy,
                        otherSyncService,
                        mTrustedVaultClient,
                        otherProfile);
        notifier.syncStateChanged();
        otherNotifier.syncStateChanged();

        // Each Profile posts its own notification, with its own PendingIntent.
        verify(mNotificationManagerProxy, Mockito.times(2))
                .notify(mNotificationWrapperCaptor.capture());
        List<NotificationWrapper> notifications = mNotificationWrapperCaptor.getAllValues();
        assertEquals(mNotificationTag, notifications.get(0).getMetadata().tag);
        assertEquals(otherNotificationTag, notifications.get(1).getMetadata().tag);
        Intent firstIntent = getContentIntent(notifications.get(0));
        Intent secondIntent = getContentIntent(notifications.get(1));
        assertTrue(ProfileTokenUtils.hasProfileToken(firstIntent));
        assertTrue(ProfileTokenUtils.hasProfileToken(secondIntent));
        assertEquals(
                mNotificationTag,
                ProfileTokenUtils.getNotificationTagFromIntent("SyncUI", firstIntent));
        assertEquals(
                otherNotificationTag,
                ProfileTokenUtils.getNotificationTagFromIntent("SyncUI", secondIntent));
        assertFalse(firstIntent.filterEquals(secondIntent));

        // Resolving the error in one Profile only clears that Profile's notification.
        when(mSyncService.isPassphraseRequiredForPreferredDataTypes()).thenReturn(false);
        notifier.syncStateChanged();
        verify(mNotificationManagerProxy).cancel(eq(mNotificationTag), anyInt());
        verify(mNotificationManagerProxy, Mockito.never())
                .cancel(eq(otherNotificationTag), anyInt());
    }

    private static Intent getContentIntent(NotificationWrapper notification) {
        Intent trampolineIntent =
                shadowOf(notification.getNotification().contentIntent).getSavedIntent();
        PendingIntent contentPendingIntent =
                NotificationIntentInterceptor.getPendingIntentForTesting(trampolineIntent);
        return shadowOf(contentPendingIntent).getSavedIntent();
    }
}
