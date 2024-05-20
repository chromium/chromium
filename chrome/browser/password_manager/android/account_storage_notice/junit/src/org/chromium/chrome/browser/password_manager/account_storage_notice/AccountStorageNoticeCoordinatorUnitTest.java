// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.account_storage_notice;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;
import java.util.Set;

/**
 * Unit tests for AccountStorageNoticeCoordinator. These are only meant to test the logic for when
 * to create the coordinator or not. For anything UI/click related, use the *IntegrationTest.java.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {
            AccountStorageNoticeCoordinatorUnitTest.ShadowBottomSheetControllerProvider.class
        })
public class AccountStorageNoticeCoordinatorUnitTest {
    @Implements(BottomSheetControllerProvider.class)
    public static class ShadowBottomSheetControllerProvider {
        private static BottomSheetController sBottomSheetController;

        @Implementation
        public static BottomSheetController from(WindowAndroid windowAndroid) {
            return sBottomSheetController;
        }

        public static void setBottomSheetController(BottomSheetController controller) {
            sBottomSheetController = controller;
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private SyncService mSyncService;
    @Mock private PrefService mPrefService;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Context mContext;
    @Mock private SettingsLauncher mSettingsLauncher;

    @Before
    public void setUp() {
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference(mContext));
        ShadowBottomSheetControllerProvider.setBottomSheetController(mBottomSheetController);
        // TODO(crbug.com/341176706): Shadow the AccountStorageNoticeView constructor instead.
        AccountStorageNoticeView.setSkipLayoutForTesting(true);
    }

    @Test
    @SmallTest
    public void testShouldNotCreateIfIncognito() {
        when(mPrefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)).thenReturn(false);

        @Nullable
        AccountStorageNoticeCoordinator coordinator =
                AccountStorageNoticeCoordinator.create(
                        /* syncService= */ null, mPrefService, mWindowAndroid, mSettingsLauncher);

        Assert.assertEquals(coordinator, null);
        verify(mPrefService, never())
                .setBoolean(eq(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN), anyBoolean());
    }

    @Test
    @SmallTest
    public void testShouldNotCreateIfSyncing() {
        when(mSyncService.hasSyncConsent()).thenReturn(true);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.PASSWORDS));
        when(mPrefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)).thenReturn(false);

        @Nullable
        AccountStorageNoticeCoordinator coordinator =
                AccountStorageNoticeCoordinator.create(
                        mSyncService, mPrefService, mWindowAndroid, mSettingsLauncher);

        Assert.assertEquals(coordinator, null);
        verify(mPrefService, never())
                .setBoolean(eq(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN), anyBoolean());
    }

    @Test
    @SmallTest
    public void testShouldNotCreateIfSignedOut() {
        when(mSyncService.hasSyncConsent()).thenReturn(false);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of());
        when(mPrefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)).thenReturn(false);

        @Nullable
        AccountStorageNoticeCoordinator coordinator =
                AccountStorageNoticeCoordinator.create(
                        mSyncService, mPrefService, mWindowAndroid, mSettingsLauncher);

        Assert.assertEquals(coordinator, null);
        verify(mPrefService, never())
                .setBoolean(eq(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN), anyBoolean());
    }

    @Test
    @SmallTest
    public void testShouldNotCreateIfPasswordsDataTypeDisabled() {
        when(mSyncService.hasSyncConsent()).thenReturn(false);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of());
        when(mPrefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)).thenReturn(false);

        @Nullable
        AccountStorageNoticeCoordinator coordinator =
                AccountStorageNoticeCoordinator.create(
                        mSyncService, mPrefService, mWindowAndroid, mSettingsLauncher);

        Assert.assertEquals(coordinator, null);
        verify(mPrefService, never())
                .setBoolean(eq(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN), anyBoolean());
    }

    @Test
    @SmallTest
    public void testShouldNotCreateIfAlreadyShown() {
        when(mSyncService.hasSyncConsent()).thenReturn(false);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.PASSWORDS));
        when(mPrefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)).thenReturn(true);

        @Nullable
        AccountStorageNoticeCoordinator coordinator =
                AccountStorageNoticeCoordinator.create(
                        mSyncService, mPrefService, mWindowAndroid, mSettingsLauncher);

        Assert.assertEquals(coordinator, null);
        verify(mPrefService, never())
                .setBoolean(eq(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN), anyBoolean());
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS)
    public void testShouldNotCreateIfFlagDisabled() {
        when(mSyncService.hasSyncConsent()).thenReturn(false);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.PASSWORDS));
        when(mPrefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)).thenReturn(false);

        @Nullable
        AccountStorageNoticeCoordinator coordinator =
                AccountStorageNoticeCoordinator.create(
                        mSyncService, mPrefService, mWindowAndroid, mSettingsLauncher);

        Assert.assertEquals(coordinator, null);
        verify(mPrefService, never())
                .setBoolean(eq(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN), anyBoolean());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS)
    public void testShouldNotCreateIfRequestShowContentFailed() {
        when(mSyncService.hasSyncConsent()).thenReturn(false);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.PASSWORDS));
        when(mPrefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)).thenReturn(false);
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(false);

        @Nullable
        AccountStorageNoticeCoordinator coordinator =
                AccountStorageNoticeCoordinator.create(
                        mSyncService, mPrefService, mWindowAndroid, mSettingsLauncher);

        Assert.assertEquals(coordinator, null);
        verify(mPrefService, never())
                .setBoolean(eq(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN), anyBoolean());
    }

    @Test
    @SmallTest
    @EnableFeatures(ChromeFeatureList.ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS)
    public void testShouldCreate() {
        when(mSyncService.hasSyncConsent()).thenReturn(false);
        when(mSyncService.getSelectedTypes()).thenReturn(Set.of(UserSelectableType.PASSWORDS));
        when(mPrefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)).thenReturn(false);
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);

        @Nullable
        AccountStorageNoticeCoordinator coordinator =
                AccountStorageNoticeCoordinator.create(
                        mSyncService, mPrefService, mWindowAndroid, mSettingsLauncher);

        Assert.assertNotEquals(coordinator, null);
        verify(mPrefService).setBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN, true);
    }
}
