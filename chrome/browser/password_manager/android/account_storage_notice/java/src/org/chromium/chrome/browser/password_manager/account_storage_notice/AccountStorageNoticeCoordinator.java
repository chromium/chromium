// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager.account_storage_notice;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.settings.SettingsLauncher.SettingsFragment;
import org.chromium.ui.base.WindowAndroid;

/** Coordinator for the UI described in account_storage_notice.h, meant to be used from native. */
class AccountStorageNoticeCoordinator {
    private final SettingsLauncher mSettingsLauncher;
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final AccountStorageNoticeView mView;
    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetStateChanged(
                        @SheetState int newState, @StateChangeReason int reason) {
                    if (newState == SheetState.HIDDEN) {
                        assert mNativeCoordinatorObserver != 0;
                        AccountStorageNoticeCoordinatorJni.get()
                                .onClosed(mNativeCoordinatorObserver);
                    }
                }
            };

    private long mNativeCoordinatorObserver;

    @CalledByNative
    public AccountStorageNoticeCoordinator(
            WindowAndroid windowAndroid,
            SettingsLauncher settingsLauncher,
            long nativeCoordinatorObserver) {
        assert nativeCoordinatorObserver != 0;
        mSettingsLauncher = settingsLauncher;
        mContext = windowAndroid.getContext().get();
        mBottomSheetController = BottomSheetControllerProvider.from(windowAndroid);
        mNativeCoordinatorObserver = nativeCoordinatorObserver;
        // No need for a ViewBinder to pass the callbacks, the logic is too simple.
        mView =
                new AccountStorageNoticeView(
                        windowAndroid.getContext().get(),
                        this::onButtonClicked,
                        this::onSettingsLinkClicked);
        mBottomSheetController.requestShowContent(mView, /* animate= */ true);
        mBottomSheetController.addObserver(mBottomSheetObserver);
    }

    @CalledByNative
    public void destroy() {
        // Avoid dangling pointer.
        mNativeCoordinatorObserver = 0;
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        if (mBottomSheetController.getCurrentSheetContent() == mView) {
            // This means the owning C++ object was destroyed while the sheet was still showing. It
            // should almost never happen. Note that it's important to remove the observer before
            // calling hideContent(), so mBottomSheetObserver isn't triggered.
            mBottomSheetController.hideContent(mView, /* animate= */ false);
        }
    }

    private void onButtonClicked() {
        mBottomSheetController.hideContent(mView, /* animate= */ true);
        // mBottomSheetObserver will take care of notifying native.
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
