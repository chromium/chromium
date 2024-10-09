// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.interstitial;

import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.DOWNLOAD_ITEM;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.PENDING_MESSAGE_IS_VISIBLE;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.PRIMARY_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.PRIMARY_BUTTON_IS_VISIBLE;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.PRIMARY_BUTTON_TEXT;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.SECONDARY_BUTTON_CALLBACK;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.SECONDARY_BUTTON_IS_VISIBLE;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.SECONDARY_BUTTON_TEXT;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.STATE;
import static org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.TITLE_TEXT;

import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.Pair;

import org.chromium.base.CallbackUtils;
import org.chromium.base.CollectionUtil;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.download.home.list.ListProperties;
import org.chromium.chrome.browser.download.home.list.ShareUtils;
import org.chromium.chrome.browser.download.home.rename.RenameDialogManager;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.chrome.browser.download.interstitial.DownloadInterstitialProperties.State;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.share.ShareHelper;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemShareInfo;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.OpenParams;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.SimpleModalDialogController;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
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

    private ModalDialogManager mModalDialogManager;

    /**
     * Creates a new DownloadInterstitialMediator instance.
     * @param contextSupplier Supplier which provides the context of the parent tab.
     * @param model A {@link PropertyModel} containing the properties defined in {@link
     *         DownloadInterstitialProperties}.
     * @param provider An {@link OfflineContentProvider} used for observing updates about the
     *         download.
     * @param snackbarManager A {@link SnackbarManager} used to display snackbars within the
     *         download interstitial view.
     * @param modalDialogManager A {@link ModalDialogManager} used to display dialogs within the
     *         download interstitial.
     */
    DownloadInterstitialMediator(
            Supplier<Context> contextSupplier,
            PropertyModel model,
            String downloadUrl,
            OfflineContentProvider provider,
            SnackbarManager snackbarManager,
            ModalDialogManager modalDialogManager) {
        mContextSupplier = contextSupplier;
        mModel = model;
        mDownloadUrl = downloadUrl;
        mProvider = provider;
        mSnackbarManager = snackbarManager;
        mModalDialogManager = modalDialogManager;

        mModel.set(ListProperties.ENABLE_ITEM_ANIMATIONS, true);
        mModel.set(ListProperties.CALLBACK_OPEN, this::onOpenItem);
        mModel.set(ListProperties.CALLBACK_PAUSE, this::onPauseItem);
        mModel.set(ListProperties.CALLBACK_RESUME, this::onResumeItem);
        mModel.set(ListProperties.CALLBACK_CANCEL, this::onCancelItem);
        mModel.set(ListProperties.CALLBACK_SHARE, this::onShareItem);
        mModel.set(ListProperties.CALLBACK_REMOVE, this::onDeleteItem);
        mModel.set(ListProperties.PROVIDER_VISUALS, (i, w, h, c) -> CallbackUtils.emptyRunnable());
        mModel.set(ListProperties.CALLBACK_RENAME, this::onRenameItem);
        mModel.set(ListProperties.CALLBACK_SELECTION, (item) -> {});

        mObserver = getOfflineContentProviderObserver();
        mProvider.addObserver(mObserver);
        UmaHelper.logUIAction(UmaHelper.Action.INITIATED);
    }

    void setModalDialogManager(ModalDialogManager modalDialogManager) {
        mModalDialogManager = modalDialogManager;
    }

    /** Destroys the mediator and its resources. */
    void destroy() {
        mProvider.removeObserver(mObserver);
    }

    private void setState(@State int state) {
        if (state == mModel.get(STATE)) return;
        mModel.set(STATE, state);
        switch (state) {
            case State.IN_PROGRESS:
                mModel.set(TITLE_TEXT, mContextSupplier.get().getString(R.string.download_started));
                mModel.set(PRIMARY_BUTTON_IS_VISIBLE, false);
                mModel.set(
                        SECONDARY_BUTTON_TEXT,
                        mContextSupplier
                                .get()
                                .getString(R.string.download_notification_cancel_button));
                mModel.set(SECONDARY_BUTTON_CALLBACK, mModel.get(ListProperties.CALLBACK_CANCEL));
                mModel.set(SECONDARY_BUTTON_IS_VISIBLE, true);
                break;
            case State.SUCCESSFUL:
                mModel.set(
                        TITLE_TEXT,
                        mContextSupplier
                                .get()
                                .getResources()
                                .getQuantityString(
                                        R.plurals.download_message_multiple_download_complete,
                                        1,
                                        1));
                mModel.set(
                        PRIMARY_BUTTON_TEXT,
                        mContextSupplier.get().getString(R.string.open_downloaded_label));
                mModel.set(PRIMARY_BUTTON_CALLBACK, mModel.get(ListProperties.CALLBACK_OPEN));
                mModel.set(PRIMARY_BUTTON_IS_VISIBLE, true);
                mModel.set(
                        SECONDARY_BUTTON_TEXT, mContextSupplier.get().getString(R.string.delete));
                mModel.set(SECONDARY_BUTTON_CALLBACK, mModel.get(ListProperties.CALLBACK_REMOVE));
                mModel.set(SECONDARY_BUTTON_IS_VISIBLE, true);
                UmaHelper.logUIAction(UmaHelper.Action.COMPLETED);
                break;
            case State.CANCELLED:
                mModel.set(TITLE_TEXT, mContextSupplier.get().getString(R.string.menu_download));
                mModel.set(
                        PRIMARY_BUTTON_TEXT,
                        mContextSupplier.get().getString(R.string.menu_download));
                mModel.set(PRIMARY_BUTTON_CALLBACK, mModel.get(ListProperties.CALLBACK_RESUME));
                mModel.set(PRIMARY_BUTTON_IS_VISIBLE, true);
                mModel.set(SECONDARY_BUTTON_IS_VISIBLE, false);
                break;
            case State.PAUSED:
                mModel.set(TITLE_TEXT, mContextSupplier.get().getString(R.string.menu_download));
                mModel.set(
                        PRIMARY_BUTTON_TEXT,
                        mContextSupplier
                                .get()
                                .getString(R.string.download_notification_resume_button));
                mModel.set(PRIMARY_BUTTON_CALLBACK, mModel.get(ListProperties.CALLBACK_RESUME));
                mModel.set(PRIMARY_BUTTON_IS_VISIBLE, true);
                mModel.set(SECONDARY_BUTTON_IS_VISIBLE, false);
                break;
            case State.PENDING:
                mModel.set(PENDING_MESSAGE_IS_VISIBLE, true);
                break;
        }
    }

    private void onOpenItem(OfflineItem item) {
        OpenParams openParams = new OpenParams(LaunchLocation.DOWNLOAD_INTERSTITIAL);
        mProvider.openItem(openParams, mModel.get(DOWNLOAD_ITEM).id);
        UmaHelper.logUIAction(UmaHelper.Action.OPENED);
    }

    private void onPauseItem(OfflineItem item) {
        mProvider.pauseDownload(mModel.get(DOWNLOAD_ITEM).id);
        UmaHelper.logUIAction(UmaHelper.Action.PAUSED);
    }

    private void onResumeItem(OfflineItem item) {
        if (mModel.get(STATE) == State.PAUSED) {
            mProvider.resumeDownload(mModel.get(DOWNLOAD_ITEM).id);
            UmaHelper.logUIAction(UmaHelper.Action.RESUMED);
        } else {
            mModel.set(STATE, State.PENDING);
            mModel.get(DownloadInterstitialProperties.RELOAD_TAB).run();
            mModel.set(DOWNLOAD_ITEM, null);
            UmaHelper.logUIAction(UmaHelper.Action.REINITIATED);
        }
    }

    private void onCancelItem(OfflineItem item) {
        setState(State.CANCELLED);
        mProvider.cancelDownload(mModel.get(DOWNLOAD_ITEM).id);
        UmaHelper.logUIAction(UmaHelper.Action.CANCELLED);
    }

    private void onDeleteItem(OfflineItem item) {
        SimpleModalDialogController modalDialogController =
                new SimpleModalDialogController(
                        mModalDialogManager,
                        (result) -> {
                            if (result == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                                showDeletedSnackbar();
                                setState(State.CANCELLED);
                                mProvider.removeItem(mModel.get(DOWNLOAD_ITEM).id);
                                UmaHelper.logUIAction(UmaHelper.Action.DELETED);
                            }
                        });
        PropertyModel properties =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, modalDialogController)
                        .with(
                                ModalDialogProperties.TITLE,
                                mContextSupplier.get().getResources().getString(R.string.delete))
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                mContextSupplier
                                        .get()
                                        .getString(
                                                R.string.confirm_delete_message,
                                                mModel.get(DOWNLOAD_ITEM).title))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mContextSupplier.get().getString(R.string.delete))
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mContextSupplier.get().getString(R.string.cancel))
                        .build();
        mModalDialogManager.showDialog(properties, ModalDialogManager.ModalDialogType.APP);
    }

    private void onShareItem(OfflineItem item) {
        shareItemsInternal(CollectionUtil.newHashSet(item));
    }

    private void onRenameItem(OfflineItem item) {
        startRename(
                mModel.get(DOWNLOAD_ITEM).title,
                (newName, renameCallback) ->
                        mProvider.renameItem(
                                mModel.get(DOWNLOAD_ITEM).id, newName, renameCallback));
    }

    private void showDeletedSnackbar() {
        Snackbar snackbar =
                Snackbar.make(
                        mContextSupplier
                                .get()
                                .getString(
                                        R.string.delete_message, mModel.get(DOWNLOAD_ITEM).title),
                        null,
                        Snackbar.TYPE_ACTION,
                        Snackbar.UMA_DOWNLOAD_INTERSTITIAL_DOWNLOAD_DELETED);
        mSnackbarManager.showSnackbar(snackbar);
    }

    private void shareItemsInternal(Collection<OfflineItem> items) {
        ShareHelper.recordShareSource(ShareHelper.ShareSourceAndroid.ANDROID_SHARE_SHEET);
        UmaHelper.logUIAction(UmaHelper.Action.SHARED);

        final Collection<Pair<OfflineItem, OfflineItemShareInfo>> shareInfo = new ArrayList<>();
        for (OfflineItem item : items) {
            mProvider.getShareInfoForItem(
                    item.id,
                    (id, info) -> {
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
            mContextSupplier
                    .get()
                    .startActivity(
                            Intent.createChooser(
                                    intent,
                                    mContextSupplier
                                            .get()
                                            .getString(R.string.share_link_chooser_title)));
        } catch (ActivityNotFoundException e) {
            Log.e(TAG, "Cannot find activity for sharing");
        } catch (Exception e) {
            Log.e(TAG, "Cannot start activity for sharing, exception: " + e);
        }
    }

    private void startRename(String name, RenameDialogManager.RenameCallback callback) {
        RenameDialogManager mRenameDialogManager =
                new RenameDialogManager(mContextSupplier.get(), mModalDialogManager);
        mRenameDialogManager.startRename(name, callback);
        UmaHelper.logUIAction(UmaHelper.Action.RENAMED);
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
                    mModel.set(PENDING_MESSAGE_IS_VISIBLE, false);
                } else if (!item.id.equals(mModel.get(DOWNLOAD_ITEM).id)) {
                    return;
                }
                mModel.set(DOWNLOAD_ITEM, item);
                switch (item.state) {
                    case OfflineItemState.IN_PROGRESS: // Intentional fallthrough.
                    case OfflineItemState.PENDING:
                        setState(State.IN_PROGRESS);
                        break;
                    case OfflineItemState.PAUSED:
                        setState(State.PAUSED);
                        break;
                    case OfflineItemState.FAILED: // Intentional fallthrough.
                    case OfflineItemState.INTERRUPTED: // Intentional fallthrough.
                    case OfflineItemState.CANCELLED: // Intentional fallthrough.
                        setState(State.CANCELLED);
                        break;
                    case OfflineItemState.COMPLETE:
                        if (mModel.get(STATE) != State.CANCELLED) {
                            setState(State.SUCCESSFUL);
                        }
                        break;
                }
            }
        };
    }

    @VisibleForTesting
    static class UmaHelper {
        /**
         * Used for logging UMA metrics about what actions the user is taking within the download
         * interstitial.
         *
         * <p>Must stay up to date with DownloadInterstitialUIAction in
         * tools/metrics/histograms/enums.xml
         */
        // TODO(crbug.com/40824284) Remove NUM_ENTRIES from the IntDef.
        @IntDef({
            Action.INITIATED,
            Action.COMPLETED,
            Action.CANCELLED,
            Action.PAUSED,
            Action.RESUMED,
            Action.OPENED,
            Action.DELETED,
            Action.REINITIATED,
            Action.SHARED,
            Action.RENAMED,
            Action.NUM_ENTRIES
        })
        @Retention(RetentionPolicy.SOURCE)
        @interface Action {
            int INITIATED = 0; // Logged when the download interstitial is opened.
            int COMPLETED = 1; // Logged when the download completes successfully.
            int CANCELLED = 2; // Logged when the download is cancelled.
            int PAUSED = 3; // Logged when the download is paused.
            int RESUMED = 4; // Logged when the download is resumed after being paused.
            int OPENED = 5; // Logged when the download is opened.
            int DELETED = 6; // Logged when the download is deleted.
            int REINITIATED = 7; // Logged when the download is re-downloaded after cancel/delete.
            int SHARED = 8; // Logged when the download is shared.
            int RENAMED = 9; // Logged when the download is renamed.
            int NUM_ENTRIES = 10;
        }

        public static void logUIAction(@Action int action) {
            RecordHistogram.recordEnumeratedHistogram(
                    "Download.Interstitial.UIAction", action, Action.NUM_ENTRIES);
        }
    }
}
