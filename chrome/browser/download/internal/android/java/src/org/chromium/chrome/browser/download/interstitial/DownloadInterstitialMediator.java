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
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.STATE;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;

import androidx.core.util.Pair;

import org.chromium.base.CollectionUtil;
import org.chromium.base.Log;
import org.chromium.chrome.browser.download.home.list.ListProperties;
import org.chromium.chrome.browser.download.home.list.ShareUtils;
import org.chromium.chrome.browser.download.home.rename.RenameDialogManager;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.State;
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

    private final Context mContext;
    private final PropertyModel mModel;
    private final OfflineContentProvider mProvider;
    private final SnackbarManager mSnackbarManager;
    private final OfflineContentProvider.Observer mObserver;
    private boolean mDownloadIsComplete;
    private boolean mPendingDeletion;

    /**
     * Creates a new DownloadInterstitialMediator instance.
     * @param context The activity context.
     * @param model A {@link PropertyModel} containing the properties defined in {@link
     *         DownloadInterstitialProperties}.
     * @param provider An {@link OfflineContentProvider} used for observing updates about the
     *         download.
     * @param snackbarManager A {@link SnackbarManager} used to display snackbars within the
     *         download interstitial view.
     */
    DownloadInterstitialMediator(Context context, PropertyModel model,
            OfflineContentProvider provider, SnackbarManager snackbarManager) {
        mContext = context;
        mModel = model;
        mProvider = provider;
        mSnackbarManager = snackbarManager;

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
        if (mPendingDeletion || mModel.get(STATE) == State.CANCELLED) {
            if (mDownloadIsComplete) {
                mProvider.removeItem(mModel.get(DOWNLOAD_ITEM).id);
            } else {
                mProvider.cancelDownload(mModel.get(DOWNLOAD_ITEM).id);
            }
        }
    }

    private void setState(@State int state) {
        mModel.set(STATE, state);
        switch (state) {
            case State.IN_PROGRESS:
                mModel.set(PRIMARY_BUTTON_IS_VISIBLE, false);
                mModel.set(SECONDARY_BUTTON_TEXT,
                        mContext.getString(R.string.download_notification_cancel_button));
                mModel.set(SECONDARY_BUTTON_CALLBACK, mModel.get(ListProperties.CALLBACK_CANCEL));
                mModel.set(SECONDARY_BUTTON_IS_VISIBLE, true);
                break;
            case State.SUCCESSFUL:
                mModel.set(PRIMARY_BUTTON_TEXT, mContext.getString(R.string.open_downloaded_label));
                mModel.set(PRIMARY_BUTTON_CALLBACK, mModel.get(ListProperties.CALLBACK_OPEN));
                mModel.set(PRIMARY_BUTTON_IS_VISIBLE, true);
                mModel.set(SECONDARY_BUTTON_TEXT, mContext.getString(R.string.delete));
                mModel.set(SECONDARY_BUTTON_CALLBACK, mModel.get(ListProperties.CALLBACK_REMOVE));
                mModel.set(SECONDARY_BUTTON_IS_VISIBLE, true);
                mDownloadIsComplete = true;
                break;
            case State.CANCELLED:
                mModel.set(PRIMARY_BUTTON_TEXT, mContext.getString(R.string.menu_download));
                mModel.set(PRIMARY_BUTTON_CALLBACK, mModel.get(ListProperties.CALLBACK_RESUME));
                mModel.set(PRIMARY_BUTTON_IS_VISIBLE, true);
                mModel.set(SECONDARY_BUTTON_IS_VISIBLE, false);
                break;
            case State.PAUSED:
                mModel.set(PRIMARY_BUTTON_TEXT,
                        mContext.getString(R.string.download_notification_resume_button));
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
        mProvider.resumeDownload(item.id, true /* hasUserGesture */);
    }

    private void onCancelItem(OfflineItem item) {
        setState(State.CANCELLED);
        mProvider.pauseDownload(item.id);
    }

    private void onDeleteItem(OfflineItem item) {
        mPendingDeletion = true;
        showDeletedSnackbar();
        setState(State.CANCELLED);
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
        Snackbar snackbar = Snackbar.make(
                mContext.getString(R.string.delete_message, mModel.get(DOWNLOAD_ITEM).title), null,
                Snackbar.TYPE_ACTION, Snackbar.UMA_DOWNLOAD_INTERSTITIAL_DOWNLOAD_DELETED);
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
            mContext.startActivity(Intent.createChooser(
                    intent, mContext.getString(R.string.share_link_chooser_title)));
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Cannot find activity for sharing");
        } catch (Exception e) {
            Log.e(TAG, "Cannot start activity for sharing, exception: " + e);
        }
    }

    private void startRename(String name, RenameDialogManager.RenameCallback callback) {
        ModalDialogManager modalDialogManager = new ModalDialogManager(
                new AppModalPresenter(mContext), ModalDialogManager.ModalDialogType.APP);
        RenameDialogManager mRenameDialogManager =
                new RenameDialogManager(mContext, modalDialogManager);
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
                if (mModel.get(DOWNLOAD_ITEM) != null
                        && !item.id.equals(mModel.get(DOWNLOAD_ITEM).id)) {
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
                        && mModel.get(STATE) != State.CANCELLED) {
                    setState(State.PAUSED);
                }
            }
        };
    }
}