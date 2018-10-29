// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.ui;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Handler;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.Toolbar.OnMenuItemClickListener;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.ViewGroup;

import org.chromium.base.CollectionUtil;
import org.chromium.base.DiscardableReferencePool;
import org.chromium.base.FileUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.home.DownloadManagerCoordinator;
import org.chromium.chrome.browser.download.home.metrics.UmaUtils;
import org.chromium.chrome.browser.download.home.metrics.UmaUtils.MenuAction;
import org.chromium.chrome.browser.download.home.toolbar.ToolbarUtils;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.native_page.BasicNativePage;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.download.DownloadPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.snackbar.SnackbarManager.SnackbarController;
import org.chromium.chrome.browser.widget.ThumbnailProvider;
import org.chromium.chrome.browser.widget.ThumbnailProviderImpl;
import org.chromium.chrome.browser.widget.selection.SelectableListLayout;
import org.chromium.chrome.browser.widget.selection.SelectableListToolbar.SearchDelegate;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.download.R;
import org.chromium.components.offline_items_collection.OfflineContentProvider;

import java.io.File;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

/**
 * Displays and manages the UI for the download manager.
 */

public class DownloadManagerUi implements OnMenuItemClickListener, SearchDelegate,
                                          BackendProvider.UIDelegate, DownloadManagerCoordinator {
    private static class DownloadBackendProvider implements BackendProvider {
        private final SelectionDelegate<DownloadHistoryItemWrapper> mSelectionDelegate;
        private final UIDelegate mUIDelegate;
        private ThumbnailProvider mThumbnailProvider;

        DownloadBackendProvider(DiscardableReferencePool referencePool, UIDelegate uiDelegate) {
            mSelectionDelegate = new DownloadItemSelectionDelegate();
            mThumbnailProvider = new ThumbnailProviderImpl(referencePool);
            mUIDelegate = uiDelegate;
        }

        // BackendProvider implementation.
        @Override
        public DownloadDelegate getDownloadDelegate() {
            return DownloadManagerService.getDownloadManagerService();
        }

        @Override
        public OfflineContentProvider getOfflineContentProvider() {
            return OfflineContentAggregatorFactory.forProfile(
                    Profile.getLastUsedProfile().getOriginalProfile());
        }

        @Override
        public ThumbnailProvider getThumbnailProvider() {
            return mThumbnailProvider;
        }

        @Override
        public SelectionDelegate<DownloadHistoryItemWrapper> getSelectionDelegate() {
            return mSelectionDelegate;
        }

        @Override
        public UIDelegate getUIDelegate() {
            return mUIDelegate;
        }

        @Override
        public void destroy() {
            mThumbnailProvider.destroy();
            mThumbnailProvider = null;
        }
    }

    private class UndoDeletionSnackbarController implements SnackbarController {
        @Override
        public void onAction(Object actionData) {
            @SuppressWarnings("unchecked")
            List<DownloadHistoryItemWrapper> items = (List<DownloadHistoryItemWrapper>) actionData;

            // Deletion was undone. Add items back to the adapter.
            mHistoryAdapter.unmarkItemsForDeletion(items);

            RecordUserAction.record("Android.DownloadManager.UndoDelete");
        }

        @Override
        public void onDismissNoAction(Object actionData) {
            @SuppressWarnings("unchecked")
            List<DownloadHistoryItemWrapper> items = (List<DownloadHistoryItemWrapper>) actionData;

            // Deletion was not undone. Remove downloads from backend.
            final ArrayList<File> filesToDelete = new ArrayList<>();

            // Some types of DownloadHistoryItemWrappers delete their own files when #remove()
            // is called. Determine which files are not deleted by the #remove() call.
            for (int i = 0; i < items.size(); i++) {
                DownloadHistoryItemWrapper wrappedItem  = items.get(i);
                if (!wrappedItem.removePermanently()) filesToDelete.add(wrappedItem.getFile());
            }

            // Delete the files associated with the download items (if necessary) using a single
            // AsyncTask that batch deletes all of the files. The thread pool has a finite
            // number of tasks that can be queued at once. If too many tasks are queued an
            // exception is thrown. See crbug.com/643811.
            // On Android M, Android DownloadManager may not delete the actual file, so we need to
            // delete the files here.
            if (filesToDelete.size() != 0) {
                new AsyncTask<Void>() {
                    @Override
                    public Void doInBackground() {
                        FileUtils.batchDeleteFiles(filesToDelete);
                        return null;
                    }
                }
                        .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
            }

            RecordUserAction.record("Android.DownloadManager.Delete");
        }
    }

    private static final int PREFETCH_BUNDLE_OPEN_DELAY_MS = 500;

    private static BackendProvider sProviderForTests;

    private final DownloadHistoryAdapter mHistoryAdapter;
    private final FilterAdapter mFilterAdapter;
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final BackendProvider mBackendProvider;
    private final SnackbarManager mSnackbarManager;

    private final UndoDeletionSnackbarController mUndoDeletionSnackbarController;
    private final RecyclerView mRecyclerView;

    private BasicNativePage mNativePage;
    private Activity mActivity;
    private ViewGroup mMainView;
    private DownloadManagerToolbar mToolbar;
    private SelectableListLayout<DownloadHistoryItemWrapper> mSelectableListLayout;
    private boolean mIsSeparateActivity;

    private int mSearchMenuId;
    private int mInfoMenuId;

    /**
     * Constructs a new DownloadManagerUi.
     * @param activity The {@link Activity} associated with the download manager.
     * @param isOffTheRecord Whether an off-the-record tab is currently being displayed.
     * @param parentComponent The {@link ComponentName} of the parent activity.
     * @param isSeparateActivity Whether the download manager UI will be shown in a separate
     *                           activity than the main Chrome activity.
     * @param snackbarManager The {@link SnackbarManager} used to display snackbars.
     */
    @SuppressWarnings({"unchecked"}) // mSelectableListLayout
    public DownloadManagerUi(Activity activity, boolean isOffTheRecord,
            ComponentName parentComponent, boolean isSeparateActivity,
            SnackbarManager snackbarManager) {
        TraceEvent.startAsync("DownloadManagerUi shown", hashCode());
        mActivity = activity;
        ChromeApplication application = (ChromeApplication) activity.getApplication();
        mBackendProvider = sProviderForTests == null
                ? new DownloadBackendProvider(application.getReferencePool(), this)
                : sProviderForTests;
        mSnackbarManager = snackbarManager;

        mMainView = (ViewGroup) LayoutInflater.from(activity).inflate(R.layout.download_main, null);

        mSelectableListLayout = (SelectableListLayout<DownloadHistoryItemWrapper>)
                mMainView.findViewById(R.id.selectable_list);

        mSelectableListLayout.initializeEmptyView(
                VectorDrawableCompat.create(
                        mActivity.getResources(), R.drawable.downloads_big, mActivity.getTheme()),
                R.string.download_manager_ui_empty, R.string.download_manager_no_results);

        mHistoryAdapter = new DownloadHistoryAdapter(isOffTheRecord, parentComponent);
        mRecyclerView = mSelectableListLayout.initializeRecyclerView(mHistoryAdapter);

        // Prevent every progress update from causing a transition animation.
        mRecyclerView.getItemAnimator().setChangeDuration(0);

        mRecyclerView.addOnScrollListener(new RecyclerView.OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                updateInfoButtonVisibility();
            }
        });

        mFilterAdapter = new FilterAdapter(mActivity.getResources());
        mFilterAdapter.initialize(this);

        boolean isLocationEnabled =
                ChromeFeatureList.isEnabled(ChromeFeatureList.DOWNLOADS_LOCATION_CHANGE);
        int normalGroupId =
                isLocationEnabled ? R.id.with_settings_normal_menu_group : R.id.normal_menu_group;
        mSearchMenuId = isLocationEnabled ? R.id.with_settings_search_menu_id : R.id.search_menu_id;
        mInfoMenuId = isLocationEnabled ? 0 : R.id.info_menu_id;

        mToolbar = (DownloadManagerToolbar) mSelectableListLayout.initializeToolbar(
                R.layout.download_manager_toolbar, mBackendProvider.getSelectionDelegate(), 0, null,
                normalGroupId, R.id.selection_mode_menu_group, R.color.modern_primary_color, this,
                true, isSeparateActivity);
        mToolbar.getMenu().setGroupVisible(normalGroupId, true);
        mToolbar.setManager(this);
        mToolbar.initialize(mFilterAdapter);

        mToolbar.initializeSearchView(this, R.string.download_manager_search, mSearchMenuId);

        mToolbar.setInfoMenuItem(mInfoMenuId);

        if (isLocationEnabled) ToolbarUtils.setupTrackerForDownloadSettingsIPH(mToolbar);

        mSelectableListLayout.configureWideDisplayStyle();
        mHistoryAdapter.initialize(mBackendProvider, mSelectableListLayout.getUiConfig());

        mUndoDeletionSnackbarController = new UndoDeletionSnackbarController();
        enableStorageInfoHeader(mHistoryAdapter.shouldShowStorageInfoHeader());

        mIsSeparateActivity = isSeparateActivity;
        if (!mIsSeparateActivity) mToolbar.removeCloseButton();

        RecordUserAction.record("Android.DownloadManager.Open");
    }

    /**
     * Sets the {@link BasicNativePage} that holds this manager.
     */
    public void setBasicNativePage(BasicNativePage delegate) {
        mNativePage = delegate;
    }

    // BackendProvider.UIDelegate implementation.
    @Override
    public void deleteItem(DownloadHistoryItemWrapper item) {
        deleteItems(CollectionUtil.newArrayList(item));
    }

    @Override
    public void shareItem(DownloadHistoryItemWrapper item) {
        shareItems(CollectionUtil.newArrayList(item));
    }

    /**
     * Called when the activity/native page is destroyed.
     */
    @Override
    public void destroy() {
        mObservers.clear();

        mFilterAdapter.destroy();
        mHistoryAdapter.destroy();

        dismissUndoDeletionSnackbars();

        mBackendProvider.destroy();

        mSelectableListLayout.onDestroyed();
        TraceEvent.finishAsync("DownloadManagerUi shown", hashCode());
    }

    /**
     * Called when the UI needs to react to the back button being pressed.
     *
     * @return Whether the back button was handled.
     */
    @Override
    public boolean onBackPressed() {
        return mSelectableListLayout.onBackPressed();
    }

    /**
     * @return The view that shows the main download UI.
     */
    @Override
    public ViewGroup getView() {
        return mMainView;
    }

    /**
     * Sets the download manager to the state that the url represents.
     */
    @Override
    public void updateForUrl(String url) {
        int filter = DownloadFilter.getFilterFromUrl(url);
        onFilterChanged(filter);
    }

    /**
     * Performs an animated expansion of the prefetch section.
     */
    @Override
    public void showPrefetchSection() {
        new Handler().postDelayed(() -> {
            mHistoryAdapter.setPrefetchSectionExpanded(true);
        }, PREFETCH_BUNDLE_OPEN_DELAY_MS);
    }

    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public boolean onMenuItemClick(MenuItem item) {
        UmaUtils.recordTopMenuAction(item.getItemId());

        if ((item.getItemId() == R.id.close_menu_id
                    || item.getItemId() == R.id.with_settings_close_menu_id)
                && mIsSeparateActivity) {
            mActivity.finish();
            return true;
        } else if (item.getItemId() == R.id.selection_mode_delete_menu_id) {
            List<DownloadHistoryItemWrapper> items =
                    mBackendProvider.getSelectionDelegate().getSelectedItemsAsList();
            mBackendProvider.getSelectionDelegate().clearSelection();

            UmaUtils.recordTopMenuDeleteCount(items.size());
            deleteItems(items);
            return true;
        } else if (item.getItemId() == R.id.selection_mode_share_menu_id) {
            List<DownloadHistoryItemWrapper> items =
                    mBackendProvider.getSelectionDelegate().getSelectedItemsAsList();
            // TODO(twellington): ideally the intent chooser would be started with
            //                    startActivityForResult() and the selection would only be cleared
            //                    after receiving an OK response. See crbug.com/638916.
            mBackendProvider.getSelectionDelegate().clearSelection();
            UmaUtils.recordTopMenuShareCount(items.size());
            shareItems(items);
            return true;
        } else if (item.getItemId() == mInfoMenuId) {
            boolean showInfo = !mHistoryAdapter.shouldShowStorageInfoHeader();
            RecordHistogram.recordEnumeratedHistogram("Android.DownloadManager.Menu.Action",
                    showInfo ? UmaUtils.MenuAction.SHOW_INFO : UmaUtils.MenuAction.HIDE_INFO,
                    MenuAction.NUM_ENTRIES);
            enableStorageInfoHeader(showInfo);
            return true;
        } else if (item.getItemId() == mSearchMenuId) {
            // The header should be removed as soon as a search is started. It will be added back in
            // DownloadHistoryAdatper#filter() when the search is ended.
            mHistoryAdapter.removeHeader();
            mSelectableListLayout.onStartSearch();
            mToolbar.showSearchView();
            return true;
        } else if (item.getItemId() == R.id.settings_menu_id) {
            Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                    mActivity, DownloadPreferences.class.getName());
            mActivity.startActivity(intent);
            return true;
        }
        return false;
    }

    /**
     * @return The activity that holds the download UI.
     */
    Activity getActivity() {
        return mActivity;
    }

    /**
     * @return The BackendProvider associated with the download UI.
     */
    public BackendProvider getBackendProvider() {
        return mBackendProvider;
    }

    /** Called when the filter has been changed by the user. */
    void onFilterChanged(@DownloadFilter.Type int filter) {
        mBackendProvider.getSelectionDelegate().clearSelection();
        mToolbar.hideSearchView();
        mToolbar.onFilterChanged(filter);
        mHistoryAdapter.onFilterChanged(filter);

        String url = DownloadFilter.getUrlForFilter(filter);
        for (Observer observer : mObservers) observer.onUrlChanged(url);

        if (mNativePage != null) {
            mNativePage.onStateChange(DownloadFilter.getUrlForFilter(filter));
        }

        RecordHistogram.recordEnumeratedHistogram(
                "Android.DownloadManager.Filter", filter, DownloadFilter.Type.NUM_ENTRIES);
    }

    @Override
    public void onSearchTextChanged(String query) {
        mHistoryAdapter.search(query);
    }

    @Override
    public void onEndSearch() {
        mSelectableListLayout.onEndSearch();
        mHistoryAdapter.onEndSearch();
    }

    private void enableStorageInfoHeader(boolean show) {
        // Finish any running or pending animations right away.
        if (mRecyclerView.getItemAnimator() != null) {
            mRecyclerView.getItemAnimator().endAnimations();
        }

        mHistoryAdapter.setShowStorageInfoHeader(show);
        mToolbar.updateInfoMenuItem(true, show);
    }

    private void startShareIntent(Intent intent) {
        mActivity.startActivity(Intent.createChooser(
                intent, mActivity.getString(R.string.share_link_chooser_title)));
    }

    private void deleteItems(List<DownloadHistoryItemWrapper> items) {
        // Build a full list of items to remove for the selected items.
        List<DownloadHistoryItemWrapper> itemsToDelete = new ArrayList<>();

        Set<String> filePathsToRemove = new HashSet<>();
        CollectionUtil.forEach(items, item -> {
            if (!filePathsToRemove.contains(item.getFilePath())) {
                Set<DownloadHistoryItemWrapper> itemsForFilePath =
                        mHistoryAdapter.getItemsForFilePath(item.getFilePath());
                if (itemsForFilePath != null) itemsToDelete.addAll(itemsForFilePath);
                filePathsToRemove.add(item.getFilePath());
            }
        });

        if (itemsToDelete.isEmpty()) return;

        mHistoryAdapter.markItemsForDeletion(itemsToDelete);

        boolean singleItemDeleted = items.size() == 1;
        String snackbarText = singleItemDeleted
                ? items.get(0).getDisplayFileName()
                : String.format(Locale.getDefault(), "%d", items.size());
        int snackbarTemplateId = singleItemDeleted ? R.string.undo_bar_delete_message
                : R.string.undo_bar_multiple_downloads_delete_message;

        Snackbar snackbar = Snackbar.make(snackbarText, mUndoDeletionSnackbarController,
                Snackbar.TYPE_ACTION, Snackbar.UMA_DOWNLOAD_DELETE_UNDO);
        snackbar.setAction(mActivity.getString(R.string.undo), itemsToDelete);
        snackbar.setTemplateText(mActivity.getString(snackbarTemplateId));

        mSnackbarManager.showSnackbar(snackbar);
    }

    private void dismissUndoDeletionSnackbars() {
        mSnackbarManager.dismissSnackbars(mUndoDeletionSnackbarController);
    }

    /**
     * @return True if info menu item should be shown on download toolbar, false otherwise.
     */
    private boolean shouldShowInfoButton() {
        return mHistoryAdapter.getItemCount() > 0 && !mToolbar.isSearching()
                && !mBackendProvider.getSelectionDelegate().isSelectionEnabled();
    }

    /**
     * Update info button visibility based on whether info header is visible on download page.
     */
    void updateInfoButtonVisibility() {
        LinearLayoutManager layoutManager = (LinearLayoutManager) mRecyclerView.getLayoutManager();
        boolean infoHeaderIsVisible = layoutManager.findFirstVisibleItemPosition() == 0;
        mToolbar.updateInfoMenuItem(infoHeaderIsVisible && shouldShowInfoButton(),
                mHistoryAdapter.shouldShowStorageInfoHeader());
    }

    @VisibleForTesting
    public SnackbarManager getSnackbarManagerForTesting() {
        return mSnackbarManager;
    }

    /** Returns the {@link DownloadManagerToolbar}. */
    @VisibleForTesting
    public DownloadManagerToolbar getDownloadManagerToolbarForTests() {
        return mToolbar;
    }

    /** Returns the {@link DownloadHistoryAdapter}. */
    @VisibleForTesting
    public DownloadHistoryAdapter getDownloadHistoryAdapterForTests() {
        return mHistoryAdapter;
    }

    /** Sets a BackendProvider that is used in place of a real one. */
    @VisibleForTesting
    public static void setProviderForTests(BackendProvider provider) {
        sProviderForTests = provider;
    }

    private void shareItems(final List<DownloadHistoryItemWrapper> items) {
        boolean done = DownloadUtils.prepareForSharing(items, (newFilePathMap) -> {
            startShareIntent(DownloadUtils.createShareIntent(items, newFilePathMap));
        });

        if (done) startShareIntent(DownloadUtils.createShareIntent(items, null));
    }
}
