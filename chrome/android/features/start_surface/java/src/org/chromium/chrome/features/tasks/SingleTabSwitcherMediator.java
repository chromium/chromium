// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import static org.chromium.chrome.features.tasks.SingleTabViewProperties.CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.FAVICON;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.IS_VISIBLE;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.TAB_THUMBNAIL;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.TITLE;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.URL;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Size;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate.TabSwitcherType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher.TabSwitcherViewObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.tasks.tab_management.ThumbnailProvider;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.chrome.features.start_surface.StartSurfaceUserData;
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
    private final TabModelObserver mNormalTabModelObserver;
    private final TabModelSelectorObserver mTabModelSelectorObserver;
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final boolean mIsSurfacePolishEnabled;
    private TabSwitcher.OnTabSelectingListener mTabSelectingListener;
    private boolean mShouldIgnoreNextSelect;
    private boolean mSelectedTabDidNotChangedAfterShown;
    private boolean mAddNormalTabModelObserverPending;
    private Long mTabTitleAvailableTime;
    private boolean mFaviconInitialized;
    private Context mContext;
    private ThumbnailProvider mThumbnailProvider;
    private Size mThumbnailSize;

    SingleTabSwitcherMediator(
            Context context,
            PropertyModel propertyModel,
            TabModelSelector tabModelSelector,
            TabListFaviconProvider tabListFaviconProvider,
            TabContentManager tabContentManager,
            boolean isSurfacePolishEnabled) {
        mTabModelSelector = tabModelSelector;
        mPropertyModel = propertyModel;
        mTabListFaviconProvider = tabListFaviconProvider;
        mContext = context;
        mIsSurfacePolishEnabled = isSurfacePolishEnabled;
        mThumbnailProvider = getThumbnailProvider(tabContentManager);
        if (mThumbnailProvider != null) {
            mThumbnailSize = getThumbnailSize(mContext);
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
                        if (!ReturnToChromeUtil.isStartSurfaceRefactorEnabled(mContext)
                                && mTabModelSelector.isIncognitoSelected()) {
                            return;
                        }

                        assert mPropertyModel.get(IS_VISIBLE);

                        mSelectedTabDidNotChangedAfterShown = false;
                        updateSelectedTab(tab);
                        if (type == TabSelectionType.FROM_CLOSE
                                || type == TabSelectionType.FROM_UNDO
                                || mShouldIgnoreNextSelect) {
                            mShouldIgnoreNextSelect = false;
                            return;
                        }
                        mTabSelectingListener.onTabSelecting(tab.getId());
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
                            if (isSurfacePolishEnabled) {
                                mPropertyModel.set(URL, tab.getUrl().getHost());
                            }
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
        if (mFaviconInitialized || !mTabModelSelector.isTabStateInitialized()) return;

        TabModel normalTabModel = mTabModelSelector.getModel(false);
        int selectedTabIndex = normalTabModel.index();
        if (selectedTabIndex != TabList.INVALID_TAB_INDEX) {
            assert normalTabModel.getCount() > 0;
            Tab tab = normalTabModel.getTabAt(selectedTabIndex);
            updateFavicon(tab);
            mayUpdateTabThumbnail(tab);
            mFaviconInitialized = true;
        }
    }

    private void updateFavicon(Tab tab) {
        assert mTabListFaviconProvider.isInitialized();
        mTabListFaviconProvider.getFaviconDrawableForUrlAsync(
                tab.getUrl(),
                false,
                (Drawable favicon) -> {
                    mPropertyModel.set(FAVICON, favicon);
                });
    }

    private void mayUpdateTabThumbnail(Tab tab) {
        if (!mIsSurfacePolishEnabled) return;

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
        mShouldIgnoreNextSelect = false;
        mTabModelSelector
                .getTabModelFilterProvider()
                .removeTabModelFilterObserver(mNormalTabModelObserver);
        mTabModelSelector.removeObserver(mTabModelSelectorObserver);

        mPropertyModel.set(IS_VISIBLE, false);
        mPropertyModel.set(FAVICON, mTabListFaviconProvider.getDefaultFaviconDrawable(false));
        mPropertyModel.set(TITLE, "");
        if (mIsSurfacePolishEnabled) {
            mPropertyModel.set(TAB_THUMBNAIL, null);
            mPropertyModel.set(URL, "");
        }

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

        if (ChromeFeatureList.sInstantStart.isEnabled()
                && !mTabModelSelector.isTabStateInitialized()) {
            mAddNormalTabModelObserverPending = true;

            PseudoTab activeTab;
            try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
                activeTab = PseudoTab.getActiveTabFromStateFile(mContext);
            }
            if (activeTab != null) {
                mPropertyModel.set(TITLE, activeTab.getTitle());
                if (mIsSurfacePolishEnabled) {
                    mPropertyModel.set(URL, activeTab.getUrl().getHost());
                }
                if (mTabTitleAvailableTime == null) {
                    mTabTitleAvailableTime = SystemClock.elapsedRealtime();
                }
            }
        } else {
            mTabModelSelector
                    .getTabModelFilterProvider()
                    .addTabModelFilterObserver(mNormalTabModelObserver);
            TabModel normalTabModel = mTabModelSelector.getModel(false);

            int selectedTabIndex = normalTabModel.index();
            if (selectedTabIndex != TabList.INVALID_TAB_INDEX) {
                assert normalTabModel.getCount() > 0;
                updateSelectedTab(normalTabModel.getTabAt(selectedTabIndex));
                if (mTabTitleAvailableTime == null) {
                    mTabTitleAvailableTime = SystemClock.elapsedRealtime();
                }
            }
        }
        mPropertyModel.set(IS_VISIBLE, true);

        for (TabSwitcherViewObserver observer : mObservers) {
            observer.startedShowing();
        }
        for (TabSwitcherViewObserver observer : mObservers) {
            observer.finishedShowing();
        }
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
                mTabTitleAvailableTime - activityCreationTimeMs,
                TabUiFeatureUtilities.supportInstantStart(false, mContext));
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
        if (mIsSurfacePolishEnabled) {
            mPropertyModel.set(URL, tab.getUrl().getHost());
            mayUpdateTabThumbnail(tab);
        }
    }

    private void selectTheCurrentTab() {
        assert !mTabModelSelector.isIncognitoSelected();
        if (mSelectedTabDidNotChangedAfterShown) {
            RecordUserAction.record("MobileTabReturnedToCurrentTab.SingleTabCard");
        }
        mTabSelectingListener.onTabSelecting(mTabModelSelector.getCurrentTabId());
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
        int size =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.single_tab_module_tab_thumbnail_size);
        return new Size(size, size);
    }
}
