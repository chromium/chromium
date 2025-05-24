// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui.bookmark_batch_upload_card;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;

import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.LifecycleObserver;
import androidx.lifecycle.LifecycleOwner;

import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.DeviceAuthSource;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.R;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.ui.BatchUploadDialogCoordinator;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.DataType;
import org.chromium.components.sync.LocalDataDescription;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.TransportState;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.HashMap;
import java.util.Set;

@NullMarked
class BookmarkBatchUploadCardMediator
        implements SyncService.SyncStateChangedListener, BatchUploadDialogCoordinator.Listener {
    private final LifecycleObserver mLifeCycleObserver =
            new DefaultLifecycleObserver() {
                @Override
                public void onResume(LifecycleOwner lifecycleOwner) {
                    immediatelyHideBatchUploadCardAndUpdateItsVisibility();
                }

                @Override
                public void onDestroy(LifecycleOwner lifecycleOwner) {
                    lifecycleOwner.getLifecycle().removeObserver(this);
                }
            };

    private final Context mContext;
    private final Profile mProfile;
    private final PropertyModel mModel;
    private final ModalDialogManager mDialogManager;
    private final SnackbarManager mSnackbarManager;
    private final ReauthenticatorBridge mReauthenticatorBridge;
    private final Runnable mBatchUploadCardChangeAction;

    private final @Nullable SyncService mSyncService;
    private @MonotonicNonNull HashMap<Integer, LocalDataDescription> mLocalDataDescriptionsMap;
    private boolean mShouldBeVisible;

    /**
     * @param activity The {@link Activity} associated with the card.
     * @param lifecycleOwner {@link LifecycleOwner} that can be used to listen for activity
     *     destruction.
     * @param modalDialogManager {@link ModalDialogManager} that can be used to display the dialog.
     * @param profile {@link Profile} that is associated with the card.
     * @param model {@link PropertyModel} that is associated with the card.
     * @param snackbarManager {@link SnackbarManager} used to display snackbars.
     * @param batchUploadCardChangeAction {@link Runnable} that is used to update the card.
     */
    public BookmarkBatchUploadCardMediator(
            Activity activity,
            LifecycleOwner lifecycleOwner,
            ModalDialogManager modalDialogManager,
            Profile profile,
            PropertyModel model,
            SnackbarManager snackbarManager,
            Runnable batchUploadCardChangeAction) {
        mContext = activity;
        mProfile = profile;
        mModel = model;
        mSnackbarManager = snackbarManager;
        mBatchUploadCardChangeAction = batchUploadCardChangeAction;
        mSyncService = SyncServiceFactory.getForProfile(mProfile);
        if (mSyncService != null) {
            mSyncService.addSyncStateChangedListener(this);
        }

        mReauthenticatorBridge =
                ReauthenticatorBridge.create(
                        activity, mProfile, DeviceAuthSource.BOOKMARK_BATCH_UPLOAD);
        mDialogManager = modalDialogManager;

        lifecycleOwner.getLifecycle().addObserver(mLifeCycleObserver);

        updateBatchUploadCard();
    }

    public void destroy() {
        if (mSyncService != null) {
            mSyncService.removeSyncStateChangedListener(this);
        }
        if (mReauthenticatorBridge != null) {
            mReauthenticatorBridge.destroy();
        }
    }

    /** Returns whether the batch upload card should be visible. */
    public boolean shouldBeVisible() {
        return mShouldBeVisible;
    }

    /**
     * To ensure a smooth user experience, the batch upload card is immediately hidden before any
     * asynchronous updates occur. This prevents a potential delay in hiding the card if the updated
     * visibility state is set to 'off', which could lead to an inconsistent UI.
     */
    public void immediatelyHideBatchUploadCardAndUpdateItsVisibility() {
        mShouldBeVisible = false;
        mBatchUploadCardChangeAction.run();
        updateBatchUploadCard();
    }

    /** {@link SyncService.SyncStateChangedListener} implementation. */
    @Override
    public void syncStateChanged() {
        updateBatchUploadCard();
    }

    @Override
    public void onSaveInAccountDialogButtonClicked(Set<Integer> types, int itemsCount) {
        if (!types.contains(DataType.PASSWORDS)) {
            uploadLocalDataAndShowSnackbar(types, itemsCount);
            return;
        }
        // Uploading passwords requires a reauthentication.
        mReauthenticatorBridge.reauthenticate(
                success -> {
                    if (success) {
                        uploadLocalDataAndShowSnackbar(types, itemsCount);
                    }
                });
    }

    private void uploadLocalDataAndShowSnackbar(Set<Integer> types, int itemsCount) {
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        assumeNonNull(syncService);
        syncService.triggerLocalDataMigration(types);
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        assumeNonNull(identityManager);
        CoreAccountInfo coreAccountInfo =
                identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        assumeNonNull(coreAccountInfo);
        String snackbarMessage =
                mContext.getResources()
                        .getQuantityString(
                                R.plurals.account_settings_bulk_upload_saved_snackbar_message,
                                itemsCount,
                                coreAccountInfo.getEmail());
        mSnackbarManager.showSnackbar(
                Snackbar.make(
                                snackbarMessage,
                                /* controller= */ null,
                                Snackbar.TYPE_ACTION,
                                Snackbar.UMA_BOOKMARK_BATCH_UPLOAD)
                        .setSingleLine(false));
        immediatelyHideBatchUploadCardAndUpdateItsVisibility();
    }

    private void updateBatchUploadCard() {
        // Calling getLocalDataDescriptions() API when sync is in configuring state should be
        // avoided. Since it will return an empty map, which could be inconsistent with the actual
        // local data. Also updateBatchUploadCard() will be triggered again after the state changes
        // from
        // CONFIGURING to ACTIVE.
        if (mSyncService == null
                || mSyncService.getTransportState() == TransportState.CONFIGURING) {
            return;
        }

        mSyncService.getLocalDataDescriptions(
                mReauthenticatorBridge.getBiometricAvailabilityStatus()
                                == BiometricStatus.UNAVAILABLE
                        ? Set.of(DataType.BOOKMARKS, DataType.READING_LIST)
                        : Set.of(DataType.BOOKMARKS, DataType.READING_LIST, DataType.PASSWORDS),
                localDataDescriptionsMap -> {
                    mLocalDataDescriptionsMap = localDataDescriptionsMap;
                    // There should be at lease one bookmark or reading list item to show the batch
                    // upload card.
                    mShouldBeVisible = countItemsNotOfType(DataType.PASSWORDS) > 0;
                    if (mShouldBeVisible) {
                        setupBatchUploadCardPropertyModel();
                    }
                    mBatchUploadCardChangeAction.run();
                });
    }

    @RequiresNonNull("mLocalDataDescriptionsMap")
    private int countItemsNotOfType(@DataType int type) {
        int ret = 0;
        for (var entry : mLocalDataDescriptionsMap.entrySet()) {
            if (entry.getKey() != type) {
                ret += entry.getValue().itemCount();
            }
        }
        return ret;
    }

    private void setupBatchUploadCardPropertyModel() {
        // TODO(crbug.com/354922852): Handle accounts with non-displayable email address.
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        assumeNonNull(identityManager);
        CoreAccountInfo accountInfo = identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        // setupBatchUploadCardView() is called asynchronously through updateBatchUploadCard(), so
        // it could be
        // called while there is no primary account.
        if (accountInfo == null) {
            return;
        }

        assumeNonNull(mLocalDataDescriptionsMap);
        mModel.set(
                BookmarkBatchUploadCardProperties.ON_CLICK_LISTENER,
                v -> {
                    BatchUploadDialogCoordinator.show(
                            mContext, mLocalDataDescriptionsMap, mDialogManager, this);
                });

        int localBookmarksCount = 0;
        LocalDataDescription bookmarksLocalDataDescription =
                mLocalDataDescriptionsMap.get(DataType.BOOKMARKS);
        if (bookmarksLocalDataDescription != null) {
            localBookmarksCount = bookmarksLocalDataDescription.itemCount();
        }

        int localItemsCountExcludingBookmarks = countItemsNotOfType(DataType.BOOKMARKS);

        if (localItemsCountExcludingBookmarks == 0) {
            mModel.set(
                    BookmarkBatchUploadCardProperties.DESCRIPTION_TEXT,
                    mContext.getResources()
                            .getQuantityString(
                                    R.plurals.bookmarks_left_behind_bookmark,
                                    localBookmarksCount,
                                    localBookmarksCount,
                                    accountInfo.getEmail()));
        } else if (localBookmarksCount == 0) {
            mModel.set(
                    BookmarkBatchUploadCardProperties.DESCRIPTION_TEXT,
                    mContext.getResources()
                            .getQuantityString(
                                    R.plurals.bookmarks_left_behind_other,
                                    localItemsCountExcludingBookmarks,
                                    localItemsCountExcludingBookmarks,
                                    accountInfo.getEmail()));
        } else {
            mModel.set(
                    BookmarkBatchUploadCardProperties.DESCRIPTION_TEXT,
                    mContext.getResources()
                            .getQuantityString(
                                    R.plurals.bookmarks_left_behind_bookmark_and_other,
                                    localBookmarksCount,
                                    localBookmarksCount,
                                    accountInfo.getEmail()));
        }
    }
}
