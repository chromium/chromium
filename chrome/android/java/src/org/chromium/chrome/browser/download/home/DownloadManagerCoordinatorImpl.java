// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home;

import android.app.Activity;
import android.content.Intent;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.download.home.filter.Filters;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.browser.download.home.list.DateOrderedListCoordinator;
import org.chromium.chrome.browser.download.home.list.DateOrderedListCoordinator.DateOrderedListObserver;
import org.chromium.chrome.browser.download.home.list.ListItem;
import org.chromium.chrome.browser.download.home.snackbars.DeleteUndoCoordinator;
import org.chromium.chrome.browser.download.home.toolbar.ToolbarCoordinator;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.download.DownloadPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate;
import org.chromium.chrome.download.R;

import java.io.Closeable;

/**
 * The top level coordinator for the download home UI.  This is currently an in progress class and
 * is not fully fleshed out yet.
 */
class DownloadManagerCoordinatorImpl
        implements DownloadManagerCoordinator, ToolbarCoordinator.ToolbarActionDelegate {
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final DateOrderedListCoordinator mListCoordinator;
    private final DeleteUndoCoordinator mDeleteCoordinator;

    private final ToolbarCoordinator mToolbarCoordinator;
    private final SelectionDelegate<ListItem> mSelectionDelegate;

    private final Activity mActivity;

    private ViewGroup mMainView;

    private boolean mMuteFilterChanges;

    /** Builds a {@link DownloadManagerCoordinatorImpl} instance. */
    public DownloadManagerCoordinatorImpl(Profile profile, Activity activity,
            DownloadManagerUiConfig config, SnackbarManager snackbarManager) {
        mActivity = activity;
        mDeleteCoordinator = new DeleteUndoCoordinator(snackbarManager);
        mSelectionDelegate = new SelectionDelegate<ListItem>();
        mListCoordinator = new DateOrderedListCoordinator(mActivity, config,
                OfflineContentAggregatorFactory.forProfile(profile),
                mDeleteCoordinator::showSnackbar, mSelectionDelegate, this ::notifyFilterChanged,
                createDateOrderedListObserver());
        mToolbarCoordinator = new ToolbarCoordinator(
                mActivity, this, mListCoordinator, mSelectionDelegate, config.isSeparateActivity);

        initializeView();
        RecordUserAction.record("Android.DownloadManager.Open");
    }

    /**
     * Creates the top level layout for download home including the toolbar.
     * TODO(crbug.com/880468) : Investigate if it is better to do in XML.
     */
    private void initializeView() {
        mMainView = new FrameLayout(mActivity);
        mMainView.setBackgroundColor(ApiCompatibilityUtils.getColor(
                mActivity.getResources(), R.color.modern_primary_color));

        FrameLayout.LayoutParams listParams = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT);
        listParams.setMargins(0,
                mActivity.getResources().getDimensionPixelOffset(R.dimen.toolbar_height_no_shadow),
                0, 0);
        mMainView.addView(mListCoordinator.getView(), listParams);

        FrameLayout.LayoutParams toolbarParams = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.WRAP_CONTENT);
        toolbarParams.gravity = Gravity.TOP;
        mMainView.addView(mToolbarCoordinator.getView(), toolbarParams);
    }

    private DateOrderedListObserver createDateOrderedListObserver() {
        return new DateOrderedListObserver() {
            @Override
            public void onListScroll(boolean canScrollUp) {
                if (mToolbarCoordinator == null) return;
                mToolbarCoordinator.setShowToolbarShadow(canScrollUp);
            }

            @Override
            public void onEmptyStateChanged(boolean isEmpty) {
                if (mToolbarCoordinator == null) return;
                mToolbarCoordinator.setSearchEnabled(!isEmpty);
            }
        };
    }

    // DownloadManagerCoordinator implementation.
    @Override
    public void destroy() {
        mDeleteCoordinator.destroy();
        mListCoordinator.destroy();
        mToolbarCoordinator.destroy();
    }

    @Override
    public View getView() {
        return mMainView;
    }

    @Override
    public boolean onBackPressed() {
        if (mListCoordinator.handleBackPressed()) return true;
        if (mToolbarCoordinator.handleBackPressed()) return true;
        return false;
    }

    @Override
    public void updateForUrl(String url) {
        try (FilterChangeBlock block = new FilterChangeBlock()) {
            mListCoordinator.setSelectedFilter(Filters.fromUrl(url));
        }
    }

    @Override
    public void showPrefetchSection() {
        updateForUrl(Filters.toUrl(Filters.FilterType.PREFETCHED));
    }

    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    // ToolbarActionDelegate implementation.
    @Override
    public void close() {
        mActivity.finish();
    }

    @Override
    public void openSettings() {
        RecordUserAction.record("Android.DownloadManager.Settings");
        Intent intent = PreferencesLauncher.createIntentForSettingsPage(
                mActivity, DownloadPreferences.class.getName());
        mActivity.startActivity(intent);
    }

    private void notifyFilterChanged(@FilterType int filter) {
        mSelectionDelegate.clearSelection();
        if (mMuteFilterChanges) return;

        String url = Filters.toUrl(filter);
        for (Observer observer : mObservers) observer.onUrlChanged(url);
    }

    /**
     * Helper class to mute state changes when processing a state change request from an external
     * source.
     */
    private class FilterChangeBlock implements Closeable {
        private final boolean mOriginalMuteFilterChanges;

        public FilterChangeBlock() {
            mOriginalMuteFilterChanges = mMuteFilterChanges;
            mMuteFilterChanges = true;
        }

        @Override
        public void close() {
            mMuteFilterChanges = mOriginalMuteFilterChanges;
        }
    }
}
