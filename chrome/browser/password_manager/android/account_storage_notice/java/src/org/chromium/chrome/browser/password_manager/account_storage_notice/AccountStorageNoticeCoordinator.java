// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.account_storage_notice;

import android.content.Context;
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
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
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

    private final WindowAndroid mWindowAndroid;
    private final AccountStorageNoticeView mView;

    private long mNativeCoordinatorObserver;
    private @CloseReason int mCloseReason = CloseReason.OTHER;
    // Whether mView is being shown. If any other sheet is being shown, this is false.
    private boolean mShowingSheet;

    // Impl note: hasChosenToSyncPasswords() and isGmsCoreUpdateRequired() can't be easily called
    // here (even if the predicates are moved to different targets to avoid cyclic dependencies,
    // that then causes issues with *UnitTest.java depending on mojo). So the booleans are plumbed
    // instead. The API is actually quite sane, all combinations of the booleans are valid.
    @CalledByNative
    public static boolean canShow(
            boolean hasSyncConsent,
            boolean hasChosenToSyncPasswords,
            boolean isGmsCoreUpdateRequired,
            PrefService prefService,
            @Nullable WindowAndroid windowAndroid) {
        return !hasSyncConsent
                && hasChosenToSyncPasswords
                && !isGmsCoreUpdateRequired
                && !prefService.getBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN)
                && windowAndroid != null
                && BottomSheetControllerProvider.from(windowAndroid) != null
                && windowAndroid.getContext().get() != null
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.ENABLE_PASSWORDS_ACCOUNT_STORAGE_FOR_NON_SYNCING_USERS);
    }

    @CalledByNative
    public static AccountStorageNoticeCoordinator createAndShow(
            WindowAndroid windowAndroid, PrefService prefService) {
        AccountStorageNoticeCoordinator coordinator =
                new AccountStorageNoticeCoordinator(windowAndroid);
        BottomSheetControllerProvider.from(windowAndroid)
                .requestShowContent(coordinator.mView, /* animate= */ true);
        prefService.setBoolean(Pref.ACCOUNT_STORAGE_NOTICE_SHOWN, true);
        return coordinator;
    }

    private AccountStorageNoticeCoordinator(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
        // Was checked in canShow() before.
        assert mWindowAndroid != null;
        @Nullable Context context = windowAndroid.getContext().get();
        // Was checked in canShow() before.
        assert context != null;
        mView =
                new AccountStorageNoticeView(
                        context, this::onButtonClicked, this::onSettingsLinkClicked);
        @Nullable
        BottomSheetController controller = BottomSheetControllerProvider.from(mWindowAndroid);
        // Was checked in canShow() before.
        assert controller != null;
        controller.addObserver(this);
    }

    @CalledByNative
    public void setObserver(long nativeCoordinatorObserver) {
        mNativeCoordinatorObserver = nativeCoordinatorObserver;
    }

    /** If the notice is still showing, hides it promptly without animation. Otherwise, no-op. */
    @CalledByNative
    public void hideImmediatelyIfShowing() {
        hideWithReason(CloseReason.EMBEDDER_REQUESTED, /* animate= */ false);
    }

    // EmptyBottomSheetObserver overrides.
    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        // This waits for the sheet to close, then stops the observation and notifies native.
        if (!mShowingSheet) {
            return;
        }
        mShowingSheet = false;

        // The observer was notified, so the controller should be alive.
        BottomSheetControllerProvider.from(mWindowAndroid).removeObserver(this);

        if (mCloseReason == CloseReason.OTHER
                && (reason == StateChangeReason.SWIPE || reason == StateChangeReason.BACK_PRESS)) {
            mCloseReason = CloseReason.USER_DISMISSED;
        }
        RecordHistogram.recordEnumeratedHistogram(
                CLOSE_REASON_METRIC, mCloseReason, CloseReason.MAX_VALUE + 1);

        if (mNativeCoordinatorObserver != 0) {
            AccountStorageNoticeCoordinatorJni.get().onClosed(mNativeCoordinatorObserver);
        }
    }

    @Override
    public void onSheetOpened(@StateChangeReason int reason) {
        // The observer was notified, so the controller should be alive.
        if (BottomSheetControllerProvider.from(mWindowAndroid).getCurrentSheetContent() == mView) {
            mShowingSheet = true;
        }
    }

    private void onButtonClicked() {
        hideWithReason(CloseReason.USER_CLICKED_GOT_IT, /* animate= */ true);
    }

    private void onSettingsLinkClicked() {
        @Nullable Context context = mWindowAndroid.getContext().get();
        if (context == null) {
            // The activity was closed, nothing else to do.
            return;
        }

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
                SettingsNavigationFactory.createSettingsNavigation()
                        .createSettingsIntent(context, fragment, fragmentArgs);
        mWindowAndroid.showIntent(intent, this::onSettingsClosed, /* errorId= */ null);
    }

    private void onSettingsClosed(int resultCode, Intent unused) {
        // Note: closing settings via user interaction should map to Activity.RESULT_CANCELED here,
        // but we want to hide the sheet no matter what `resultCode` is.
        hideWithReason(CloseReason.USER_CLICKED_SETTINGS, /* animate= */ true);
    }

    private void hideWithReason(@CloseReason int closeReason, boolean animate) {
        @Nullable
        BottomSheetController controller = BottomSheetControllerProvider.from(mWindowAndroid);
        if (controller == null) {
            // There isn't even a sheet controller anymore, nothing else to do.
            return;
        }

        if (controller.getCurrentSheetContent() != mView) {
            // Don't hide a different sheet.
            return;
        }

        mCloseReason = closeReason;
        controller.hideContent(mView, animate);
        // onSheetClosed() will take care of notifying native and recording the metric.
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
