// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.account_storage_notice;

import android.content.Context;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher.SettingsFragment;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.ui.base.WindowAndroid;

/** Coordinator for the UI described in account_storage_notice.h, meant to be used from native. */
class AccountStorageNoticeCoordinator extends EmptyBottomSheetObserver {
    private final SettingsLauncher mSettingsLauncher;
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final AccountStorageNoticeView mView;

    private long mNativeCoordinatorObserver;

    @CalledByNative
    public static @Nullable AccountStorageNoticeCoordinator create(
            @Nullable SyncService syncService,
            PrefService prefService,
            WindowAndroid windowAndroid,
            SettingsLauncher settingsLauncher) {
        boolean shouldShow =
                syncService != null
                        && !syncService.hasSyncConsent()
                        && syncService.getSelectedTypes().contains(UserSelectableType.PASSWORDS)
                        && !prefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)
                        && ChromeFeatureList.isEnabled(
                                ChromeFeatureList
                                        .ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS);
        if (!shouldShow) {
            return null;
        }

        BottomSheetController controller = BottomSheetControllerProvider.from(windowAndroid);
        AccountStorageNoticeView view =
                new AccountStorageNoticeView(windowAndroid.getContext().get());
        boolean success = controller.requestShowContent(view, /* animate= */ true);
        if (!success) {
            return null;
        }

        prefService.setBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN, true);
        AccountStorageNoticeCoordinator coordinator =
                new AccountStorageNoticeCoordinator(
                        windowAndroid.getContext().get(), controller, view, settingsLauncher);
        return coordinator;
    }

    private AccountStorageNoticeCoordinator(
            Context context,
            BottomSheetController bottomSheetController,
            AccountStorageNoticeView view,
            SettingsLauncher settingsLauncher) {
        mSettingsLauncher = settingsLauncher;
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mView = view;
        mView.setButtonCallback(this::onButtonClicked);
        mView.setSettingsLinkCallback(this::onSettingsLinkClicked);
        mBottomSheetController.addObserver(this);
    }

    @CalledByNative
    public void setObserver(long nativeCoordinatorObserver) {
        mNativeCoordinatorObserver = nativeCoordinatorObserver;
    }

    /** If the notice is still showing, hides it promptly without animation. Otherwise, no-op. */
    @CalledByNative
    public void hideImmediatelyIfShowing() {
        if (mBottomSheetController.getCurrentSheetContent() == mView) {
            mBottomSheetController.hideContent(mView, /* animate= */ false);
        }
    }

    // EmptyBottomSheetObserver overrides.
    @Override
    public void onSheetStateChanged(@SheetState int newState, @StateChangeReason int reason) {
        // This waits for the sheet to close, then stops the observation and notifies native.
        if (newState == SheetState.HIDDEN) {
            mBottomSheetController.removeObserver(this);
            if (mNativeCoordinatorObserver != 0) {
                AccountStorageNoticeCoordinatorJni.get().onClosed(mNativeCoordinatorObserver);
            }
        }
    }

    private void onButtonClicked() {
        mBottomSheetController.hideContent(mView, /* animate= */ true);
        // onSheetStateChanged() will take care of notifying native.
    }

    private void onSettingsLinkClicked() {
        mSettingsLauncher.launchSettingsActivity(mContext, SettingsFragment.GOOGLE_SERVICES);
    }

    @NativeMethods
    interface Natives {
        // See docs in account_storage_notice.h as to when this should be called.
        void onClosed(long nativeCoordinatorObserver);
    }
}
