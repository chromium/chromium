// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui.batch_upload_card;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;

import androidx.lifecycle.DefaultLifecycleObserver;
import androidx.lifecycle.LifecycleObserver;
import androidx.lifecycle.LifecycleOwner;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.RequiresNonNull;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.DeviceAuthSource;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.ProfileDataCache;
import org.chromium.chrome.browser.sync.R;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.sync.ui.BatchUploadDialogCoordinator;
import org.chromium.chrome.browser.sync.ui.batch_upload_card.BatchUploadCardCoordinator.EntryPoint;
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
class BatchUploadCardMediator
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
    private final OneshotSupplier<SnackbarManager> mSnackbarManagerSupplier;
    private final ReauthenticatorBridge mReauthenticatorBridge;
    private final Runnable mBatchUploadCardChangeAction;
    private final @EntryPoint int mEntryPoint;
    private final IdentityManager mIdentityManager;

    private final ProfileDataCache mProfileDataCache;
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
    public BatchUploadCardMediator(
            Activity activity,
            LifecycleOwner lifecycleOwner,
            ModalDialogManager modalDialogManager,
            Profile profile,
            PropertyModel model,
            OneshotSupplier<SnackbarManager> snackbarManagerSupplier,
            Runnable batchUploadCardChangeAction,
            @EntryPoint int entryPoint) {
        mContext = activity;
        mProfile = profile;
        mModel = model;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mBatchUploadCardChangeAction = batchUploadCardChangeAction;
        mEntryPoint = entryPoint;
        mIdentityManager =
                assumeNonNull(IdentityServicesProvider.get().getIdentityManager(mProfile));
        mProfileDataCache =
                ProfileDataCache.createWithDefaultImageSizeAndNoBadge(mContext, mIdentityManager);
        mSyncService = SyncServiceFactory.getForProfile(mProfile);
        if (mSyncService != null) {
            mSyncService.addSyncStateChangedListener(this);
        }

        mReauthenticatorBridge =
                ReauthenticatorBridge.create(
                        activity,
                        mProfile,
                        entryPoint == EntryPoint.BOOKMARK_MANAGER
                                ? DeviceAuthSource.BOOKMARK_BATCH_UPLOAD
                                : DeviceAuthSource.SETTINGS_BATCH_UPLOAD);
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
        CoreAccountInfo coreAccountInfo =
                mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        assumeNonNull(coreAccountInfo);
        // TODO(crbug.com/354922852): Handle accounts with non-displayable email address.
        String snackbarMessage =
                mContext.getResources()
                        .getQuantityString(
                                R.plurals.batch_upload_saved_snackbar_message,
                                itemsCount,
                                coreAccountInfo.getEmail());
        SnackbarManager snackbarManager = assumeNonNull(mSnackbarManagerSupplier.get());
        snackbarManager.showSnackbar(
                Snackbar.make(
                                snackbarMessage,
                                /* controller= */ null,
                                Snackbar.TYPE_ACTION,
                                mEntryPoint == EntryPoint.BOOKMARK_MANAGER
                                        ? Snackbar.UMA_BOOKMARK_BATCH_UPLOAD
                                        : Snackbar.UMA_SETTINGS_BATCH_UPLOAD)
                        .setDefaultLines(false));
        immediatelyHideBatchUploadCardAndUpdateItsVisibility();
    }

    private void updateBatchUploadCard() {
        // Calling getLocalDataDescriptions() API when sync is in configuring state should be
        // avoided. Since it will return an empty map, which could be inconsistent with the actual
        // local data. Also updateBatchUploadCard() will be triggered again after the state changes
        // from CONFIGURING to ACTIVE.
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
                    // EntryPoint.BOOKMARK_MANAGER: There should be at least one bookmark or reading
                    // list item to show the batch upload card.
                    mShouldBeVisible =
                            countItemsNotOfType(
                                            mEntryPoint == EntryPoint.BOOKMARK_MANAGER
                                                    ? DataType.PASSWORDS
                                                    : DataType.UNSPECIFIED)
                                    > 0;
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
        CoreAccountInfo accountInfo = mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN);
        // setupBatchUploadCardView() is called asynchronously through updateBatchUploadCard(), so
        // it could be called while there is no primary account.
        if (accountInfo == null) {
            return;
        }

        assumeNonNull(mLocalDataDescriptionsMap);
        mModel.set(
                BatchUploadCardProperties.ON_CLICK_LISTENER,
                v -> {
                    BatchUploadDialogCoordinator.show(
                            mContext,
                            mLocalDataDescriptionsMap,
                            mDialogManager,
                            /* displayableProfileData= */ mProfileDataCache.getProfileDataOrDefault(
                                    accountInfo.getEmail()),
                            this);
                });

        int entryPointDataType =
                mEntryPoint == EntryPoint.BOOKMARK_MANAGER
                        ? DataType.BOOKMARKS
                        : DataType.PASSWORDS;

        int localDataTypeItemsCount = 0;
        LocalDataDescription dataTypeLocalDataDescription =
                mLocalDataDescriptionsMap.get(entryPointDataType);
        if (dataTypeLocalDataDescription != null) {
            localDataTypeItemsCount = dataTypeLocalDataDescription.itemCount();
        }

        int localItemsCountExcludingEntryPointDataType = countItemsNotOfType(entryPointDataType);

        if (localItemsCountExcludingEntryPointDataType == 0) {
            mModel.set(
                    BatchUploadCardProperties.DESCRIPTION_TEXT,
                    mContext.getResources()
                            .getQuantityString(
                                    mEntryPoint == EntryPoint.BOOKMARK_MANAGER
                                            ? R.plurals.batch_upload_card_description_bookmark
                                            : R.plurals.batch_upload_card_description_password,
                                    localDataTypeItemsCount,
                                    localDataTypeItemsCount));
        } else if (localDataTypeItemsCount == 0) {
            mModel.set(
                    BatchUploadCardProperties.DESCRIPTION_TEXT,
                    mContext.getResources()
                            .getQuantityString(
                                    R.plurals.batch_upload_card_description_other,
                                    localItemsCountExcludingEntryPointDataType,
                                    localItemsCountExcludingEntryPointDataType));
        } else {
            mModel.set(
                    BatchUploadCardProperties.DESCRIPTION_TEXT,
                    mContext.getResources()
                            .getQuantityString(
                                    mEntryPoint == EntryPoint.BOOKMARK_MANAGER
                                            ? R.plurals
                                                    .batch_upload_card_description_bookmark_and_other
                                            : R.plurals
                                                    .batch_upload_card_description_password_and_other,
                                    localDataTypeItemsCount,
                                    localDataTypeItemsCount));
        }
    }
}
