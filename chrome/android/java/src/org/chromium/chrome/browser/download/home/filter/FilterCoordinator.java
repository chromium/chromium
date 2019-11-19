// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.filter;

import android.content.Context;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.home.filter.Filters.FilterType;
import org.chromium.chrome.browser.download.home.filter.chips.ChipsCoordinator;
import org.chromium.chrome.browser.offlinepages.prefetch.PrefetchConfiguration;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar;
import org.chromium.chrome.download.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A Coordinator responsible for showing the tab filter selection UI for downloads home. */
public class FilterCoordinator {
    @IntDef({TabType.FILES, TabType.PREFETCH})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TabType {
        int FILES = 0;
        int PREFETCH = 1;
    }

    /** An Observer to notify when the selected tab has changed. */
    public interface Observer {
        /** Called when the selected tab has changed. */
        void onFilterChanged(@FilterType int selectedTab);
    }

    private static Boolean sPrefetchUserSettingValueForTesting;

    private final ObserverList<Observer> mObserverList = new ObserverList<>();
    private final PropertyModel mModel = new PropertyModel(FilterProperties.ALL_KEYS);
    private final FilterView mView;

    private final ChipsCoordinator mChipsCoordinator;
    private final FilterChipsProvider mChipsProvider;

    private PrefChangeRegistrar mPrefChangeRegistrar;

    /**
     * Builds a new FilterCoordinator.
     * @param context The context to build the views and pull parameters from.
     */
    public FilterCoordinator(Context context, OfflineItemFilterSource chipFilterSource,
            DownloadManagerUiConfig config) {
        mChipsProvider =
                new FilterChipsProvider(context, type -> handleChipSelected(), chipFilterSource);
        mChipsCoordinator = new ChipsCoordinator(context, mChipsProvider);

        mView = new FilterView(context);
        PropertyModelChangeProcessor.create(mModel, mView, new FilterViewBinder());

        mModel.set(FilterProperties.CHANGE_LISTENER, this::handleTabSelected);
        selectTab(TabType.FILES);

        mModel.set(FilterProperties.SHOW_TABS, isPrefetchTabEnabled());
        setTabTitles(context, config.showOfflineHome);

        addPrefetchUserSettingsObserver();
    }

    private void addPrefetchUserSettingsObserver() {
        if (sPrefetchUserSettingValueForTesting != null) return;

        mPrefChangeRegistrar = new PrefChangeRegistrar();
        mPrefChangeRegistrar.addObserver(
                Pref.OFFLINE_PREFETCH_USER_SETTING_ENABLED, new PrefChangeRegistrar.PrefObserver() {
                    @Override
                    public void onPreferenceChange() {
                        mModel.set(FilterProperties.SHOW_TABS, isPrefetchTabEnabled());
                        int selectedTab = mModel.get(FilterProperties.SELECTED_TAB);
                        if (!isPrefetchTabEnabled()) selectedTab = TabType.FILES;
                        handleTabSelected(selectedTab);
                    }
                });
    }

    /** Tears down this coordinator. */
    public void destroy() {
        if (mPrefChangeRegistrar != null) mPrefChangeRegistrar.destroy();
    }

    /** @return The {@link View} representing this widget. */
    public View getView() {
        return mView.getView();
    }

    /** Registers {@code observer} to be notified of tab selection changes. */
    public void addObserver(Observer observer) {
        mObserverList.addObserver(observer);
    }

    /** Unregisters {@code observer} from tab selection changes. */
    public void removeObserver(Observer observer) {
        mObserverList.removeObserver(observer);
    }

    /** For testing only. */
    public static void setPrefetchUserSettingValueForTesting(boolean enabled) {
        sPrefetchUserSettingValueForTesting = enabled;
    }

    /**
     * Pushes a selected filter onto this {@link FilterCoordinator}.  This is used when external
     * components might need to update the UI state.
     */
    public void setSelectedFilter(@FilterType int filter) {
        @TabType
        int tabSelected;
        if (filter == Filters.FilterType.PREFETCHED && isPrefetchTabEnabled()) {
            tabSelected = TabType.PREFETCH;
        } else {
            mChipsProvider.setFilterSelected(filter);
            tabSelected = TabType.FILES;
        }

        handleTabSelected(tabSelected);
    }

    /** Sets the tab titles. */
    private void setTabTitles(Context context, boolean showOfflineHomeTabs) {
        mModel.set(FilterProperties.FILES_TAB_TITLE,
                context.getString(showOfflineHomeTabs ? R.string.menu_downloads
                                                      : R.string.download_manager_files_tab));
        mModel.set(FilterProperties.PREFETCH_TAB_TITLE,
                context.getString(showOfflineHomeTabs
                                ? R.string.download_manager_explore_offline
                                : R.string.ntp_article_suggestions_section_header));
    }

    private void selectTab(@TabType int selectedTab) {
        mModel.set(FilterProperties.SELECTED_TAB, selectedTab);

        if (selectedTab == TabType.FILES) {
            mModel.set(FilterProperties.CONTENT_VIEW, mChipsCoordinator.getView());
        } else if (selectedTab == TabType.PREFETCH) {
            mModel.set(FilterProperties.CONTENT_VIEW, null);
        }
    }

    private void handleTabSelected(@TabType int selectedTab) {
        selectTab(selectedTab);

        @FilterType
        int filterType;
        if (selectedTab == TabType.FILES) {
            filterType = mChipsProvider.getSelectedFilter();
        } else {
            filterType = Filters.FilterType.PREFETCHED;
        }

        notifyFilterChanged(filterType);
    }

    private void notifyFilterChanged(@FilterType int filter) {
        for (Observer observer : mObserverList) observer.onFilterChanged(filter);
    }

    private void handleChipSelected() {
        handleTabSelected(mModel.get(FilterProperties.SELECTED_TAB));
    }

    private static boolean isPrefetchTabEnabled() {
        return sPrefetchUserSettingValueForTesting == null
                ? PrefetchConfiguration.isPrefetchingFlagEnabled()
                        && PrefetchConfiguration.isPrefetchingEnabledInSettings()
                : sPrefetchUserSettingValueForTesting;
    }
}
