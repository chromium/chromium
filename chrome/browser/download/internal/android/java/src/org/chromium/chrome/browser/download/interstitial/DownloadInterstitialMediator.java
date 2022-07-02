// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.interstitial;

import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.DOWNLOAD_ITEM;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.PRIMARY_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.PRIMARY_BUTTON_IS_VISIBLE;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.PRIMARY_BUTTON_TEXT;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.SECONDARY_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.SECONDARY_BUTTON_IS_VISIBLE;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.SECONDARY_BUTTON_TEXT;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.SHOULD_REMOVE_PENDING_MESSAGE;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.STATE;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.TITLE_TEXT;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;

import androidx.core.util.Pair;

import org.chromium.base.CollectionUtil;
import org.chromium.base.Log;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.download.home.list.ListProperties;
import org.chromium.chrome.browser.download.home.list.ShareUtils;
import org.chromium.chrome.browser.download.home.rename.RenameDialogManager;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.State;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemShareInfo;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.OpenParams;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * Mediator for download interstitials. Handles internal state, event callbacks and interacts with
 * the {@link OfflineContentProvider} to update the UI based on the download's progress.
 */
class DownloadInterstitialMediator {
    private static final String TAG = "DownloadInterstitial";

    private final Supplier<Context> mContextSupplier;
    private final PropertyModel mModel;
    private final String mDownloadUrl;
    private final OfflineContentProvider mProvider;
    private final SnackbarManager mSnackbarManager;
    private final OfflineContentProvider.Observer mObserver;
    private final SharedPreferencesManager mSharedPrefs;
    private final Runnable mCloseRunnable;
    private boolean mDownloadIsComplete;
    private boolean mPendingDeletion;

    /**
     * Creates a new DownloadInterstitialMediator instance.
     * @param contextSupplier Supplier which provides the context of the parent tab.
     * @param model A {@link PropertyModel} containing the properties defined in {@link
     *         DownloadInterstitialProperties}.
     * @param provider An {@link OfflineContentProvider} used for observing updates about the
     *         download.
     * @param snackbarManager A {@link SnackbarManager} used to display snackbars within the
     *         download interstitial view.
     */
    DownloadInterstitialMediator(Supplier<Context> contextSupplier, PropertyModel model,
            String downloadUrl, OfflineContentProvider provider, SnackbarManager snackbarManager,
            SharedPreferencesManager sharedPrefs, Runnable closeRunnable) {
        mContextSupplier = contextSupplier;
        mModel = model;
        mDownloadUrl = downloadUrl;
        mProvider = provider;
        mSnackbarManager = snackbarManager;
        mSharedPrefs = sharedPrefs;
        mCloseRunnable = closeRunnable;

        mModel.set(ListProperties.ENABLE_ITEM_ANIMATIONS, true);
        mModel.set(ListProperties.CALLBACK_OPEN, this::onOpenItem);
        mModel.set(ListProperties.CALLBACK_PAUSE, this::onPauseItem);
        mModel.set(ListProperties.CALLBACK_RESUME, this::onResumeItem);
        mModel.set(ListProperties.CALLBACK_CANCEL, this::onCancelItem);
        mModel.set(ListProperties.CALLBACK_SHARE, this::onShareItem);
        mModel.set(ListProperties.CALLBACK_REMOVE, this::onDeleteItem);
        mModel.set(ListProperties.PROVIDER_VISUALS, (i, w, h, c) -> (() -> {}));
        mModel.set(ListProperties.CALLBACK_RENAME, this::onRenameItem);

        mObserver = getOfflineContentProviderObserver();
        mProvider.addObserver(mObserver);
    }

    /**
     * Destroys the mediator and its resources. Also removes the download if it has been
     * cancelled or is pending deletion.
     */
    void destroy() {
        mProvider.removeObserver(mObserver);
        if (mPendingDeletion || mModel.get(STATE) == State.PENDING_REMOVAL) {
            mProvider.removeItem(mModel.get(DOWNLOAD_ITEM).id);
        }
        clearDownloadPendingRemoval();
    }

    private void setState(@State int state) {
        mModel.set(STATE, state);
        switch (state) {
            case State.IN_PROGRESS:
                mModel.set(TITLE_TEXT, mContextSupplier.get().getString(R.string.download_started));
                mModel.set(PRIMARY_BUTTON_IS_VISIBLE, false);
                mModel.set(SECONDARY_BUTTON_TEXT,
                        mContextSupplier.get().getString(
                                R.string.download_notification_cancel_button));
                mModel.set(SECONDARY_BUTTON_CALLBACK, mModel.get(ListProperties.CALLBACK_CANCEL));
                mModel.set(SECONDARY_BUTTON_IS_VISIBLE, true);
                break;
            case State.SUCCESSFUL:
                mModel.set(TITLE_TEXT,
                        mContextSupplier.get().getResources().getQuantityString(
                                R.plurals.download_message_multiple_download_complete, 1));
                mModel.set(PRIMARY_BUTTON_TEXT,
                        mContextSupplier.get().getString(R.string.open_downloaded_label));
                mModel.set(PRIMARY_BUTTON_CALLBACK, mModel.get(ListProperties.CALLBACK_OPEN));
                mModel.set(PRIMARY_BUTTON_IS_VISIBLE, true);
                mModel.set(
                        SECONDARY_BUTTON_TEXT, mContextSupplier.get().getString(R.string.delete));
                mModel.set(SECONDARY_BUTTON_CALLBACK, mModel.get(ListProperties.CALLBACK_REMOVE));
                mModel.set(SECONDARY_BUTTON_IS_VISIBLE, true);
                mDownloadIsComplete = true;
                break;
            case State.PENDING_REMOVAL:
                mModel.set(TITLE_TEXT, mContextSupplier.get().getString(R.string.menu_download));
                mModel.set(PRIMARY_BUTTON_TEXT,
                        mContextSupplier.get().getString(R.string.menu_download));
                mModel.set(PRIMARY_BUTTON_CALLBACK, mModel.get(ListProperties.CALLBACK_RESUME));
                mModel.set(PRIMARY_BUTTON_IS_VISIBLE, true);
                mModel.set(SECONDARY_BUTTON_IS_VISIBLE, false);
                break;
            case State.PAUSED:
                mModel.set(TITLE_TEXT, mContextSupplier.get().getString(R.string.menu_download));
                mModel.set(PRIMARY_BUTTON_TEXT,
                        mContextSupplier.get().getString(
                                R.string.download_notification_resume_button));
                mModel.set(PRIMARY_BUTTON_CALLBACK, mModel.get(ListProperties.CALLBACK_RESUME));
                mModel.set(PRIMARY_BUTTON_IS_VISIBLE, true);
                mModel.set(SECONDARY_BUTTON_IS_VISIBLE, false);
                break;
        }
    }

    private void onOpenItem(OfflineItem item) {
        OpenParams openParams = new OpenParams(LaunchLocation.DOWNLOAD_INTERSTITIAL);
        mProvider.openItem(openParams, item.id);
    }

    private void onPauseItem(OfflineItem item) {
        mProvider.pauseDownload(item.id);
    }

    private void onResumeItem(OfflineItem item) {
        setState(mDownloadIsComplete ? State.SUCCESSFUL : State.IN_PROGRESS);
        mPendingDeletion = false;
        clearDownloadPendingRemoval();
        mProvider.resumeDownload(item.id, true /* hasUserGesture */);
    }

    private void onCancelItem(OfflineItem item) {
        storeDownloadPendingRemoval(item.id);
        mProvider.pauseDownload(item.id);
        setState(State.PENDING_REMOVAL);
    }

    private void onDeleteItem(OfflineItem item) {
        mPendingDeletion = true;
        storeDownloadPendingRemoval(item.id);
        showDeletedSnackbar();
        setState(State.PENDING_REMOVAL);
    }

    private void onShareItem(OfflineItem item) {
        shareItemsInternal(CollectionUtil.newHashSet(item));
    }

    private void onRenameItem(OfflineItem item) {
        startRename(mModel.get(DOWNLOAD_ITEM).title,
                (newName,
                        renameCallback) -> mProvider.renameItem(item.id, newName, renameCallback));
    }

    private void showDeletedSnackbar() {
        Snackbar snackbar = Snackbar.make(mContextSupplier.get().getString(R.string.delete_message,
                                                  mModel.get(DOWNLOAD_ITEM).title),
                null, Snackbar.TYPE_ACTION, Snackbar.UMA_DOWNLOAD_INTERSTITIAL_DOWNLOAD_DELETED);
        mSnackbarManager.showSnackbar(snackbar);
    }

    private void shareItemsInternal(Collection<OfflineItem> items) {
        final Collection<Pair<OfflineItem, OfflineItemShareInfo>> shareInfo = new ArrayList<>();
        for (OfflineItem item : items) {
            mProvider.getShareInfoForItem(item.id, (id, info) -> {
                shareInfo.add(Pair.create(item, info));

                // When we've gotten callbacks for all items, create and share the intent.
                if (shareInfo.size() == items.size()) {
                    Intent intent = ShareUtils.createIntent(shareInfo);
                    if (intent != null) startShareIntent(intent);
                }
            });
        }
    }

    private void startShareIntent(Intent intent) {
        try {
            mContextSupplier.get().startActivity(Intent.createChooser(
                    intent, mContextSupplier.get().getString(R.string.share_link_chooser_title)));
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Cannot find activity for sharing");
        } catch (Exception e) {
            Log.e(TAG, "Cannot start activity for sharing, exception: " + e);
        }
    }

    private void startRename(String name, RenameDialogManager.RenameCallback callback) {
        ModalDialogManager modalDialogManager =
                new ModalDialogManager(new AppModalPresenter(mContextSupplier.get()),
                        ModalDialogManager.ModalDialogType.APP);
        RenameDialogManager mRenameDialogManager =
                new RenameDialogManager(mContextSupplier.get(), modalDialogManager);
        mRenameDialogManager.startRename(name, callback);
    }

    private OfflineContentProvider.Observer getOfflineContentProviderObserver() {
        return new OfflineContentProvider.Observer() {
            @Override
            public void onItemsAdded(List<OfflineItem> items) {}

            @Override
            public void onItemRemoved(ContentId id) {}

            @Override
            public void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {
                if (mModel.get(DOWNLOAD_ITEM) == null) {
                    if (!TextUtils.equals(mDownloadUrl, item.originalUrl.getSpec())) return;
                    // Run before download is first attached.
                    mModel.set(SHOULD_REMOVE_PENDING_MESSAGE, true);

                } else if (!item.id.equals(mModel.get(DOWNLOAD_ITEM).id)) {
                    return;
                }
                mModel.set(DOWNLOAD_ITEM, item);

                if (item.state == OfflineItemState.IN_PROGRESS
                        && mModel.get(STATE) != State.IN_PROGRESS) {
                    setState(State.IN_PROGRESS);
                } else if (item.state == OfflineItemState.COMPLETE
                        && mModel.get(STATE) != State.SUCCESSFUL) {
                    setState(State.SUCCESSFUL);
                } else if (item.state == OfflineItemState.PAUSED
                        && mModel.get(STATE) != State.PAUSED
                        && mModel.get(STATE) != State.PENDING_REMOVAL) {
                    setState(State.PAUSED);
                } else if (item.state == OfflineItemState.CANCELLED
                        && mModel.get(STATE) != State.PENDING_REMOVAL) {
                    mCloseRunnable.run();
                }
            }
        };
    }

    private void storeDownloadPendingRemoval(ContentId downloadId) {
        final String key = ChromePreferenceKeys.DOWNLOAD_INTERSTITIAL_DOWNLOAD_PENDING_REMOVAL;
        boolean success = mSharedPrefs.writeStringSync(
                key, String.format("%s,%s", downloadId.namespace, downloadId.id));

        if (!success) {
            // Write synchronously because it might be used on restart and needs to stay
            // up-to-date.
            Log.e(TAG, "Failed to write DownloadInfo " + key);
        }
    }

    private void clearDownloadPendingRemoval() {
        final String key = ChromePreferenceKeys.DOWNLOAD_INTERSTITIAL_DOWNLOAD_PENDING_REMOVAL;
        boolean success = mSharedPrefs.removeKeySync(key);

        if (!success) {
            // Write synchronously because it might be used on restart and needs to stay
            // up-to-date.
            Log.e(TAG, "Failed to clear DownloadInfo " + key);
        }
    }
}