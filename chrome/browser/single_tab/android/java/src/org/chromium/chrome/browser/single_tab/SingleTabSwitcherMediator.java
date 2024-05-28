// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.single_tab;

import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.CLICK_LISTENER;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.FAVICON;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.TAB_THUMBNAIL;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.TITLE;
import static org.chromium.chrome.browser.single_tab.SingleTabViewProperties.URL;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Size;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tab_ui.TabSwitcher.OnTabSelectingListener;
import org.chromium.chrome.browser.tab_ui.TabSwitcher.TabSwitcherType;
import org.chromium.chrome.browser.tab_ui.TabSwitcher.TabSwitcherViewObserver;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.HostSurface;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.chrome.features.start_surface.StartSurfaceUserData;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/** Mediator of the single tab tab switcher. */
public class SingleTabSwitcherMediator implements TabSwitcher.Controller {
    @VisibleForTesting
    public static final String SINGLE_TAB_TITLE_AVAILABLE_TIME_UMA = "SingleTabTitleAvailableTime";

    private final ObserverList<TabSwitcherViewObserver> mObservers = new ObserverList<>();
    private final TabModelSelector mTabModelSelector;
    private final PropertyModel mPropertyModel;
    private final TabListFaviconProvider mTabListFaviconProvider;
    private TabModelObserver mNormalTabModelObserver;
    private TabModelSelectorObserver mTabModelSelectorObserver;
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private TabSwitcher.OnTabSelectingListener mTabSelectingListener;
    private boolean mShouldIgnoreNextSelect;
    private boolean mSelectedTabDidNotChangedAfterShown;
    private boolean mAddNormalTabModelObserverPending;
    private Long mTabTitleAvailableTime;
    private boolean mFaviconInitialized;
    private Context mContext;
    private ThumbnailProvider mThumbnailProvider;
    private Size mThumbnailSize;

    @Nullable private ModuleDelegate mModuleDelegate;

    SingleTabSwitcherMediator(
            Context context,
            PropertyModel propertyModel,
            TabModelSelector tabModelSelector,
            TabListFaviconProvider tabListFaviconProvider,
            TabContentManager tabContentManager,
            @Nullable Callback<Integer> singleTabCardClickedCallback,
            @Nullable ModuleDelegate moduleDelegate) {
        mTabModelSelector = tabModelSelector;
        mPropertyModel = propertyModel;
        mTabListFaviconProvider = tabListFaviconProvider;
        mContext = context;
        mThumbnailProvider = getThumbnailProvider(tabContentManager);
        if (mThumbnailProvider != null) {
            mThumbnailSize = getThumbnailSize(mContext);
        }
        mModuleDelegate = moduleDelegate;
        if (singleTabCardClickedCallback != null
                && mModuleDelegate != null
                && mModuleDelegate.getHostSurfaceType() == HostSurface.START_SURFACE) {
            // When the feature magic stack is enabled and shown on Start surface, we wrap the
            // singleTabCardClickedCallback as the mTabSelectingListener which is notified if the
            // single tab card is clicked.
            mTabSelectingListener = singleTabCardClickedCallback::onResult;
        }

        mPropertyModel.set(FAVICON, mTabListFaviconProvider.getDefaultFaviconDrawable(false));
        mPropertyModel.set(
                CLICK_LISTENER,
                v -> {
                    if (mTabSelectingListener != null
                            && mTabModelSelector.getCurrentTabId() != TabList.INVALID_TAB_INDEX) {
                        StartSurfaceUserData.setOpenedFromStart(mTabModelSelector.getCurrentTab());
                        selectTheCurrentTab();
                        BrowserUiUtils.recordModuleClickHistogram(
                                BrowserUiUtils.HostSurface.START_SURFACE,
                                ModuleTypeOnStartAndNtp.SINGLE_TAB_CARD);
                    }
                });

        mNormalTabModelObserver =
                new TabModelObserver() {
                    @Override
                    public void didSelectTab(Tab tab, int type, int lastId) {
                        assert mPropertyModel.get(IS_VISIBLE) || mModuleDelegate != null;

                        mSelectedTabDidNotChangedAfterShown = false;
                        if (mModuleDelegate != null
                                && type == TabSelectionType.FROM_CLOSE
                                && UrlUtilities.isNtpUrl(tab.getUrl())) {
                            // When the single tab card is shown as a module, it needs to change its
                            // visibility if the previously selected Tab is deleted and the newly
                            // selected Tab is a NTP.
                            moduleDelegate.removeModule(getModuleType());
                        } else {
                            updateSelectedTab(tab);
                        }

                        if (type == TabSelectionType.FROM_CLOSE
                                || type == TabSelectionType.FROM_UNDO
                                || mShouldIgnoreNextSelect) {
                            mShouldIgnoreNextSelect = false;
                            return;
                        }
                        // When the single tab card is shown on magic stack, it doesn't need to
                        // notify its mTabSelectingListener, but only update its own card. This is
                        // because the host surface of the magic stack has its own TabModelObserver.
                        if (mModuleDelegate == null && mTabSelectingListener != null) {
                            mTabSelectingListener.onTabSelecting(tab.getId());
                        }
                    }
                };
        mTabModelSelectorObserver =
                new TabModelSelectorObserver() {
                    @Override
                    public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                        if (!newModel.isIncognito()) mShouldIgnoreNextSelect = true;
                    }

                    @Override
                    public void onTabStateInitialized() {
                        TabModel normalTabModel = mTabModelSelector.getModel(false);
                        if (mAddNormalTabModelObserverPending) {
                            mAddNormalTabModelObserverPending = false;
                            mTabModelSelector
                                    .getTabModelFilterProvider()
                                    .addTabModelFilterObserver(mNormalTabModelObserver);
                        }

                        int selectedTabIndex = normalTabModel.index();
                        if (selectedTabIndex != TabList.INVALID_TAB_INDEX) {
                            assert normalTabModel.getCount() > 0;

                            Tab tab = normalTabModel.getTabAt(selectedTabIndex);
                            mPropertyModel.set(TITLE, tab.getTitle());
                            mPropertyModel.set(URL, getDomainUrl(tab.getUrl()));
                            if (mTabTitleAvailableTime == null) {
                                mTabTitleAvailableTime = SystemClock.elapsedRealtime();
                            }
                            // Favicon should be updated here unless mTabListFaviconProvider hasn't
                            // been initialized yet.
                            assert !mFaviconInitialized;
                            if (mTabListFaviconProvider.isInitialized()) {
                                mFaviconInitialized = true;
                                updateFavicon(tab);
                                mayUpdateTabThumbnail(tab);
                            }
                        }
                    }
                };
    }

    void initWithNative() {
        if (mFaviconInitialized || !mTabModelSelector.isTabStateInitialized()) {
            return;
        }

        mTabListFaviconProvider.initWithNative(
                mTabModelSelector.getModel(/* isIncognito= */ false).getProfile());
        mFaviconInitialized = true;
        TabModel normalTabModel = mTabModelSelector.getModel(false);
        int selectedTabIndex = normalTabModel.index();
        if (selectedTabIndex != TabList.INVALID_TAB_INDEX) {
            assert normalTabModel.getCount() > 0;
            Tab tab = normalTabModel.getTabAt(selectedTabIndex);
            updateFavicon(tab);
            mayUpdateTabThumbnail(tab);
        }
    }

    private void updateFavicon(Tab tab) {
        if (!mTabListFaviconProvider.isInitialized()) return;

        mTabListFaviconProvider.getFaviconDrawableForUrlAsync(
                tab.getUrl(),
                false,
                (Drawable favicon) -> {
                    mPropertyModel.set(FAVICON, favicon);
                });
    }

    private void mayUpdateTabThumbnail(Tab tab) {
        mThumbnailProvider.getTabThumbnailWithCallback(
                tab.getId(),
                mThumbnailSize,
                (Bitmap tabThumbnail) -> {
                    mPropertyModel.set(TAB_THUMBNAIL, tabThumbnail);
                },
                /* forceUpdate= */ true,
                /* writeToCache= */ true,
                /* isSelected= */ false);
    }

    void setOnTabSelectingListener(TabSwitcher.OnTabSelectingListener listener) {
        mTabSelectingListener = listener;
    }

    // Controller implementation
    @Override
    public void addTabSwitcherViewObserver(TabSwitcherViewObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeTabSwitcherViewObserver(TabSwitcherViewObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void prepareHideTabSwitcherView() {}

    @Override
    public void hideTabSwitcherView(boolean animate) {
        if (!mPropertyModel.get(IS_VISIBLE)) return;

        mShouldIgnoreNextSelect = false;
        mTabModelSelector
                .getTabModelFilterProvider()
                .removeTabModelFilterObserver(mNormalTabModelObserver);
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);

        mPropertyModel.set(IS_VISIBLE, false);
        mPropertyModel.set(FAVICON, mTabListFaviconProvider.getDefaultFaviconDrawable(false));
        mPropertyModel.set(TITLE, "");
        mPropertyModel.set(TAB_THUMBNAIL, null);
        mPropertyModel.set(URL, "");

        for (TabSwitcherViewObserver observer : mObservers) {
            observer.startedHiding();
        }
        for (TabSwitcherViewObserver observer : mObservers) {
            observer.finishedHiding();
        }
    }

    @Override
    public void showTabSwitcherView(boolean animate) {
        mSelectedTabDidNotChangedAfterShown = true;
        mTabModelSelector.addObserver(mTabModelSelectorObserver);

        mTabModelSelector
                .getTabModelFilterProvider()
                .addTabModelFilterObserver(mNormalTabModelObserver);
        TabModel normalTabModel = mTabModelSelector.getModel(false);

        int selectedTabIndex = normalTabModel.index();
        if (selectedTabIndex != TabList.INVALID_TAB_INDEX) {
            assert normalTabModel.getCount() > 0;

            Tab activeTab = normalTabModel.getTabAt(selectedTabIndex);
            maybeNotifyDataAvailable(activeTab.getUrl());
            updateSelectedTab(activeTab);

            if (mTabTitleAvailableTime == null) {
                mTabTitleAvailableTime = SystemClock.elapsedRealtime();
            }
        }
        mPropertyModel.set(IS_VISIBLE, true);

        // When the single tab module is shown in the magic stack, there isn't any observer any
        // more, early exits here.
        if (mModuleDelegate != null) return;

        for (TabSwitcherViewObserver observer : mObservers) {
            observer.startedShowing();
        }
        for (TabSwitcherViewObserver observer : mObservers) {
            observer.finishedShowing();
        }
    }

    /**
     * Notifies the {@link mModuleDelegate} whether there is data to show if exists.
     *
     * @param currentTabUrl The Url of the current Tab.
     */
    private void maybeNotifyDataAvailable(GURL currentTabUrl) {
        if (mModuleDelegate == null) return;

        // When the single tab module is shown in the magic stack,
        if (UrlUtilities.isNtpUrl(currentTabUrl)) {
            mModuleDelegate.onDataFetchFailed(getModuleType());
            hideTabSwitcherView(false);
            return;
        }

        mModuleDelegate.onDataReady(getModuleType(), mPropertyModel);
    }

    void showModule() {
        assert mModuleDelegate != null;
        initWithNative();
        showTabSwitcherView(false);
    }

    @Override
    public boolean onBackPressed() {
        // The singe tab switcher shouldn't handle any back button. The back button will be handled
        // by the ChromeTabbedActivity. See https://crbug.com/1187714.
        return false;
    }

    @Override
    public @BackPressResult int handleBackPress() {
        return BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressChangedSupplier;
    }

    @Override
    public void onOverviewShownAtLaunch(long activityCreationTimeMs) {
        if (mTabTitleAvailableTime == null) return;

        StartSurfaceConfiguration.recordHistogram(
                SINGLE_TAB_TITLE_AVAILABLE_TIME_UMA,
                mTabTitleAvailableTime - activityCreationTimeMs);
    }

    @Override
    public boolean isDialogVisible() {
        return false;
    }

    @Override
    public ObservableSupplier<Boolean> isDialogVisibleSupplier() {
        return new ObservableSupplierImpl<>();
    }

    @Override
    public @TabSwitcherType int getTabSwitcherType() {
        return TabSwitcherType.SINGLE;
    }

    @Override
    public void onHomepageChanged() {}

    private void updateSelectedTab(Tab tab) {
        if (tab.isLoading() && TextUtils.isEmpty(tab.getTitle())) {
            TabObserver tabObserver =
                    new EmptyTabObserver() {
                        @Override
                        public void onPageLoadFinished(Tab tab, GURL url) {
                            super.onPageLoadFinished(tab, url);
                            mPropertyModel.set(TITLE, tab.getTitle());
                            tab.removeObserver(this);
                        }
                    };
            tab.addObserver(tabObserver);
        } else {
            mPropertyModel.set(TITLE, tab.getTitle());
        }
        mTabListFaviconProvider.getFaviconDrawableForUrlAsync(
                tab.getUrl(),
                false,
                (Drawable favicon) -> {
                    mPropertyModel.set(FAVICON, favicon);
                });
        mPropertyModel.set(URL, getDomainUrl(tab.getUrl()));
        mayUpdateTabThumbnail(tab);
    }

    private void selectTheCurrentTab() {
        assert !mTabModelSelector.isIncognitoSelected();
        if (mSelectedTabDidNotChangedAfterShown) {
            RecordUserAction.record("MobileTabReturnedToCurrentTab.SingleTabCard");
        }
        if (mTabSelectingListener != null) {
            mTabSelectingListener.onTabSelecting(mTabModelSelector.getCurrentTabId());
        }
    }

    static ThumbnailProvider getThumbnailProvider(TabContentManager tabContentManager) {
        if (tabContentManager == null) return null;

        return (tabId, thumbnailSize, callback, forceUpdate, writeBack, isSelected) -> {
            tabContentManager.getTabThumbnailWithCallback(
                    tabId, thumbnailSize, callback, forceUpdate, writeBack);
        };
    }

    @VisibleForTesting
    public static Size getThumbnailSize(Context context) {
        int resourceId =
                StartSurfaceConfiguration.useMagicStack()
                        ? org.chromium.chrome.browser.tab_ui.R.dimen
                                .single_tab_module_tab_thumbnail_size_big
                        : R.dimen.single_tab_module_tab_thumbnail_size;
        int size = context.getResources().getDimensionPixelSize(resourceId);
        return new Size(size, size);
    }

    @ModuleType
    int getModuleType() {
        return ModuleDelegate.ModuleType.SINGLE_TAB;
    }

    static String getDomainUrl(GURL url) {
        if (StartSurfaceConfiguration.useMagicStack()) {
            String domainUrl = UrlUtilities.getDomainAndRegistry(url.getSpec(), false);
            return !TextUtils.isEmpty(domainUrl) ? domainUrl : url.getHost();
        } else {
            return url.getHost();
        }
    }

    OnTabSelectingListener getTabSelectingListenerForTesting() {
        return mTabSelectingListener;
    }
}
