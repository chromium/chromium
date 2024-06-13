// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.account_storage_notice;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.account_storage_toggle.AccountStorageToggleFragmentArgs;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher.SettingsFragment;
import org.chromium.components.prefs.PrefService;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Coordinator for the UI described in account_storage_notice.h, meant to be used from native. */
class AccountStorageNoticeCoordinator extends EmptyBottomSheetObserver {
    // The reason the notice was closed.
    // These values are persisted to logs. Entries should not be renumbered and numeric values
    // should never be reused. Keep in sync with AndroidAccountStorageNoticeCloseReason in
    // tools/metrics/histograms/metadata/password/enums.xml.
    @IntDef({
        CloseReason.OTHER,
        CloseReason.USER_DISMISSED,
        CloseReason.USER_CLICKED_GOT_IT,
        CloseReason.USER_CLICKED_SETTINGS,
        CloseReason.EMBEDDER_REQUESTED,
    })
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    public @interface CloseReason {
        int OTHER = 0;
        // Dismissed by swiping or back pressing.
        int USER_DISMISSED = 1;
        int USER_CLICKED_GOT_IT = 2;
        int USER_CLICKED_SETTINGS = 3;
        // Embedder requested the notice to close via hideImmediatelyIfShowing(). In practice this
        // means the tab was closed.
        int EMBEDDER_REQUESTED = 4;
        int MAX_VALUE = EMBEDDER_REQUESTED;
    }

    @VisibleForTesting
    public static final String CLOSE_REASON_METRIC =
            "PasswordManager.AndroidAccountStorageNotice.CloseReason";

    private final SettingsLauncher mSettingsLauncher;
    private final WindowAndroid mWindowAndroid;
    private final AccountStorageNoticeView mView;

    private long mNativeCoordinatorObserver;
    private @CloseReason int mCloseReason = CloseReason.OTHER;

    // Impl note: hasChosenToSyncPasswords() and isGmsCoreUpdateRequired() can't be easily called
    // here (even if the predicates are moved to different targets to avoid cyclic dependencies,
    // that then causes issues with *UnitTest.java depending on mojo). So the booleans are plumbed
    // instead. The API is actually quite sane, all combinations of the booleans are valid.
    @CalledByNative
    public static @Nullable AccountStorageNoticeCoordinator create(
            boolean hasSyncConsent,
            boolean hasChosenToSyncPasswords,
            boolean isGmsCoreUpdateRequired,
            PrefService prefService,
            WindowAndroid windowAndroid,
            SettingsLauncher settingsLauncher) {
        boolean shouldShow =
                !hasSyncConsent
                        && hasChosenToSyncPasswords
                        && !isGmsCoreUpdateRequired
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
                new AccountStorageNoticeCoordinator(windowAndroid, view, settingsLauncher);
        return coordinator;
    }

    private AccountStorageNoticeCoordinator(
            WindowAndroid windowAndroid,
            AccountStorageNoticeView view,
            SettingsLauncher settingsLauncher) {
        mSettingsLauncher = settingsLauncher;
        mWindowAndroid = windowAndroid;
        mView = view;
        mView.setButtonCallback(this::onButtonClicked);
        mView.setSettingsLinkCallback(this::onSettingsLinkClicked);
        BottomSheetControllerProvider.from(mWindowAndroid).addObserver(this);
    }

    @CalledByNative
    public void setObserver(long nativeCoordinatorObserver) {
        mNativeCoordinatorObserver = nativeCoordinatorObserver;
    }

    /** If the notice is still showing, hides it promptly without animation. Otherwise, no-op. */
    @CalledByNative
    public void hideImmediatelyIfShowing() {
        BottomSheetController controller = BottomSheetControllerProvider.from(mWindowAndroid);
        if (controller.getCurrentSheetContent() == mView) {
            mCloseReason = CloseReason.EMBEDDER_REQUESTED;
            controller.hideContent(mView, /* animate= */ false);
        }
    }

    // EmptyBottomSheetObserver overrides.
    @Override
    public void onSheetStateChanged(@SheetState int newState, @StateChangeReason int reason) {
        // This waits for the sheet to close, then stops the observation and notifies native.
        if (newState == SheetState.HIDDEN) {
            BottomSheetControllerProvider.from(mWindowAndroid).removeObserver(this);

            if (mCloseReason == CloseReason.OTHER
                    && (reason == StateChangeReason.SWIPE
                            || reason == StateChangeReason.BACK_PRESS)) {
                mCloseReason = CloseReason.USER_DISMISSED;
            }
            RecordHistogram.recordEnumeratedHistogram(
                    CLOSE_REASON_METRIC, mCloseReason, CloseReason.MAX_VALUE + 1);

            if (mNativeCoordinatorObserver != 0) {
                AccountStorageNoticeCoordinatorJni.get().onClosed(mNativeCoordinatorObserver);
            }
        }
    }

    private void onButtonClicked() {
        mCloseReason = CloseReason.USER_CLICKED_GOT_IT;
        BottomSheetControllerProvider.from(mWindowAndroid).hideContent(mView, /* animate= */ true);
        // onSheetStateChanged() will take care of notifying native.
    }

    private void onSettingsLinkClicked() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putBoolean(AccountStorageToggleFragmentArgs.HIGHLIGHT, true);
        // The toggle to disable account storage lives on different fragments depending on the flag.
        @SettingsFragment
        int fragment =
                ChromeFeatureList.isEnabled(
                                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
                        ? SettingsFragment.MANAGE_SYNC
                        : SettingsFragment.GOOGLE_SERVICES;
        Intent intent =
                mSettingsLauncher.createSettingsActivityIntent(
                        mWindowAndroid.getContext().get(), fragment, fragmentArgs);
        mWindowAndroid.showIntent(intent, this::onSettingsClosed, /* errorId= */ null);
    }

    private void onSettingsClosed(int resultCode, Intent unused) {
        // Note: closing settings via user interaction should map to Activity.RESULT_CANCELED here,
        // but we want to hide the sheet no matter what `resultCode` is.
        mCloseReason = CloseReason.USER_CLICKED_SETTINGS;
        BottomSheetControllerProvider.from(mWindowAndroid).hideContent(mView, /* animate= */ true);
        // onSheetStateChanged() will take care of notifying native.
    }

    @NativeMethods
    interface Natives {
        // See docs in account_storage_notice.h as to when this should be called.
        void onClosed(long nativeCoordinatorObserver);
    }

    public View getBottomSheetViewForTesting() {
        return mView.getContentView();
    }
}
