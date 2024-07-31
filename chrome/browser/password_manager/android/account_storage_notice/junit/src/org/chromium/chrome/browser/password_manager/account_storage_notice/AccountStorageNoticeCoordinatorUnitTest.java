// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.account_storage_notice;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.when;

import android.content.Context;

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
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

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
@EnableFeatures(ChromeFeatureList.ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS)
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
    @Mock private PrefService mPrefService;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Context mContext;

    @Before
    public void setUp() {
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference(mContext));
        ShadowBottomSheetControllerProvider.setBottomSheetController(mBottomSheetController);
    }

    @Test
    @SmallTest
    public void testShouldNotCreateIfNotSyncingPasswords() {
        when(mPrefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)).thenReturn(false);

        boolean canShow =
                AccountStorageNoticeCoordinator.canShow(
                        /* hasSyncConsent= */ false,
                        /* hasChosenToSyncPasswords= */ false,
                        /* isGmsCoreUpdateRequired= */ false,
                        mPrefService,
                        mWindowAndroid);

        Assert.assertFalse(canShow);
    }

    @Test
    @SmallTest
    public void testShouldNotCreateIfHasSyncConsent() {
        when(mPrefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)).thenReturn(false);

        boolean canShow =
                AccountStorageNoticeCoordinator.canShow(
                        /* hasSyncConsent= */ true,
                        /* hasChosenToSyncPasswords= */ true,
                        /* isGmsCoreUpdateRequired= */ false,
                        mPrefService,
                        mWindowAndroid);

        Assert.assertFalse(canShow);
    }

    @Test
    @SmallTest
    public void testShouldNotCreateIfGmsCoreOutdated() {
        when(mPrefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)).thenReturn(false);

        boolean canShow =
                AccountStorageNoticeCoordinator.canShow(
                        /* hasSyncConsent= */ false,
                        /* hasChosenToSyncPasswords= */ true,
                        /* isGmsCoreUpdateRequired= */ true,
                        mPrefService,
                        mWindowAndroid);

        Assert.assertFalse(canShow);
    }

    @Test
    @SmallTest
    public void testShouldNotCreateIfAlreadyShown() {
        when(mPrefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)).thenReturn(true);

        boolean canShow =
                AccountStorageNoticeCoordinator.canShow(
                        /* hasSyncConsent= */ false,
                        /* hasChosenToSyncPasswords= */ true,
                        /* isGmsCoreUpdateRequired= */ false,
                        mPrefService,
                        mWindowAndroid);

        Assert.assertFalse(canShow);
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS)
    public void testShouldNotCreateIfFlagDisabled() {
        when(mPrefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)).thenReturn(false);

        boolean canShow =
                AccountStorageNoticeCoordinator.canShow(
                        /* hasSyncConsent= */ false,
                        /* hasChosenToSyncPasswords= */ true,
                        /* isGmsCoreUpdateRequired= */ false,
                        mPrefService,
                        mWindowAndroid);

        Assert.assertFalse(canShow);
    }

    @Test
    @SmallTest
    public void testShouldNotCreateIfNoWindowAndroid() {
        when(mPrefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)).thenReturn(false);

        boolean canShow =
                AccountStorageNoticeCoordinator.canShow(
                        /* hasSyncConsent= */ false,
                        /* hasChosenToSyncPasswords= */ true,
                        /* isGmsCoreUpdateRequired= */ false,
                        mPrefService,
                        /* windowAndroid= */ null);

        Assert.assertFalse(canShow);
    }

    @Test
    @SmallTest
    public void testShouldNotCreateIfNoBottomSheetController() {
        when(mPrefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)).thenReturn(false);
        ShadowBottomSheetControllerProvider.setBottomSheetController(null);

        boolean canShow =
                AccountStorageNoticeCoordinator.canShow(
                        /* hasSyncConsent= */ false,
                        /* hasChosenToSyncPasswords= */ true,
                        /* isGmsCoreUpdateRequired= */ false,
                        mPrefService,
                        mWindowAndroid);

        Assert.assertFalse(canShow);
    }

    @Test
    @SmallTest
    public void testShouldNotCreateIfNoContext() {
        when(mPrefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)).thenReturn(false);
        when(mWindowAndroid.getContext()).thenReturn(new WeakReference(null));

        boolean canShow =
                AccountStorageNoticeCoordinator.canShow(
                        /* hasSyncConsent= */ false,
                        /* hasChosenToSyncPasswords= */ true,
                        /* isGmsCoreUpdateRequired= */ false,
                        mPrefService,
                        mWindowAndroid);

        Assert.assertFalse(canShow);
    }

    @Test
    @SmallTest
    public void testShouldCreate() {
        when(mPrefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)).thenReturn(false);
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);

        boolean canShow =
                AccountStorageNoticeCoordinator.canShow(
                        /* hasSyncConsent= */ false,
                        /* hasChosenToSyncPasswords= */ true,
                        /* isGmsCoreUpdateRequired= */ false,
                        mPrefService,
                        mWindowAndroid);

        Assert.assertTrue(canShow);
    }
}
