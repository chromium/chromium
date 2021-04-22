// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.content.res.Resources;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ListItemType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

/**
 * Business logic for the UI component of Continuous Search Navigation. This class updates the UI on
 * search result updates.
 */
class ContinuousSearchListMediator implements ContinuousNavigationUserDataObserver, Callback<Tab>,
                                              ThemeColorProvider.ThemeColorObserver {
    private final ModelList mModelList;
    private final PropertyModel mRootViewModel;
    private final Callback<Boolean> mSetLayoutVisibility;
    private final ThemeColorProvider mThemeColorProvider;
    private final Resources mResources;
    private Tab mCurrentTab;
    private boolean mOnSrp;
    private ContinuousNavigationUserDataImpl mCurrentUserData;
    private @PageCategory int mPageCategory;
    private boolean mVisible;
    private boolean mScrolled;
    // The navigation index when CSN metadata was retrieved.
    private int mStartNavigationIndex;

    ContinuousSearchListMediator(ModelList modelList, PropertyModel rootViewModel,
            Callback<Boolean> setLayoutVisibility, ThemeColorProvider themeColorProvider,
            Resources resources) {
        mModelList = modelList;
        mRootViewModel = rootViewModel;
        mSetLayoutVisibility = setLayoutVisibility;
        mThemeColorProvider = themeColorProvider;
        mResources = resources;

        mRootViewModel.set(ContinuousSearchListProperties.DISMISS_CLICK_CALLBACK,
                (v) -> invalidateOnUserRequest());
        if (mThemeColorProvider != null) {
            mThemeColorProvider.addThemeColorObserver(this);
            int themeColor = mThemeColorProvider.getThemeColor();
            mRootViewModel.set(ContinuousSearchListProperties.BACKGROUND_COLOR, themeColor);
            mRootViewModel.set(ContinuousSearchListProperties.FOREGROUND_COLOR,
                    shouldUseDarkElementColors(themeColor)
                            ? getColor(R.color.default_icon_color_dark)
                            : getColor(R.color.default_icon_color_light));
        }
    }

    private void invalidateOnUserRequest() {
        // |mCurrentUserData| should *almost* always be non-null here. This is because we invalidate
        // the UI immediately after nullifying |mCurrentUserData|.
        // There might be a rare race condition where the user manages to click the dismiss button
        // after |mCurrentUserData| is nullified and before the UI is invalidated. In that case,
        // |#invalidateOnUserRequest| will be no-op. However, the UI will be dismissed eventually
        // when |#onInvalidate| is called.
        if (mCurrentUserData != null) mCurrentUserData.invalidateData();
    }

    /**
     * Called on observing a new tab.
     */
    @Override
    public void onResult(Tab tab) {
        if (mCurrentUserData != null) {
            mCurrentUserData.removeObserver(this);
            mCurrentUserData = null;
        }

        onInvalidate();
        mCurrentTab = tab;
        if (mCurrentTab == null) return;

        mCurrentUserData = ContinuousNavigationUserDataImpl.getOrCreateForTab(mCurrentTab);
        mCurrentUserData.addObserver(this);
    }

    @Override
    public void onInvalidate() {
        mModelList.clear();
        setVisibility(false);
        mOnSrp = false;
    }

    @Override
    public void onUpdate(ContinuousNavigationMetadata metadata) {
        mModelList.clear();

        mPageCategory = metadata.getCategory();
        // We need to know the current navigation index because we want to come back here when the
        // provider label is clicked.
        if (mCurrentTab != null && mCurrentTab.getWebContents() != null
                && mCurrentTab.getWebContents().getNavigationController() != null) {
            mStartNavigationIndex = mCurrentTab.getWebContents()
                                            .getNavigationController()
                                            .getLastCommittedEntryIndex();
        } else {
            mStartNavigationIndex = -1;
        }
        String providerName = metadata.getProviderName();
        if (!TextUtils.isEmpty(providerName)) {
            String providerLabel = mResources.getString(R.string.csn_provider_label, providerName);
            mModelList.add(new ListItem(
                    ListItemType.GROUP_LABEL, generateListItem(providerLabel, null, 0, true)));
        }

        int resultCount = 0;
        for (PageGroup group : metadata.getGroups()) {
            int itemType = group.isAdGroup() ? ListItemType.AD : ListItemType.SEARCH_RESULT;
            for (PageItem result : group.getPageItems()) {
                mModelList.add(new ListItem(itemType,
                        generateListItem(
                                result.getTitle(), result.getUrl(), resultCount++, false)));
            }
        }
    }

    @Override
    public void onUrlChanged(GURL currentUrl, boolean onSrp) {
        mOnSrp = onSrp;
        for (ListItem listItem : mModelList) {
            if (listItem.type == ListItemType.GROUP_LABEL) continue;

            boolean isSelected = currentUrl != null
                    && currentUrl.equals(listItem.model.get(ContinuousSearchListProperties.URL));
            listItem.model.set(ContinuousSearchListProperties.IS_SELECTED, isSelected);
        }
        setVisibility(mModelList.size() > 0 && !mOnSrp);
    }

    /**
     * Generates a list item with the given attributes.
     * @param text            Displayed as the primary text.
     * @param url             Displayed as teh secondary text.
     * @param resultPosition  Denotes the position of this result in the list.
     * @param isProviderLabel Whether this is the item that shows the provider information.
     * @return {@link PropertyModel} representing this item.
     */
    private PropertyModel generateListItem(
            String text, GURL url, int resultPosition, boolean isProviderLabel) {
        int backgroundColor =
                getBackgroundColorForParentBackgroundColor(mThemeColorProvider.getThemeColor());
        boolean useDarkColors = shouldUseDarkElementColors(backgroundColor);
        return new PropertyModel.Builder(ContinuousSearchListProperties.ITEM_KEYS)
                .with(ContinuousSearchListProperties.LABEL, text)
                .with(ContinuousSearchListProperties.URL, url)
                .with(ContinuousSearchListProperties.IS_SELECTED, false)
                .with(ContinuousSearchListProperties.BORDER_COLOR,
                        useDarkColors ? getColor(R.color.default_icon_color_dark)
                                      : getColor(R.color.default_icon_color_light))
                .with(ContinuousSearchListProperties.CLICK_LISTENER,
                        (view) -> handleItemClick(url, resultPosition, isProviderLabel))
                .with(ContinuousSearchListProperties.BACKGROUND_COLOR, backgroundColor)
                .with(ContinuousSearchListProperties.TITLE_TEXT_STYLE,
                        useDarkColors ? R.style.TextAppearance_TextMedium_Primary_Dark
                                      : R.style.TextAppearance_TextMedium_Primary_Light)
                .with(ContinuousSearchListProperties.DESCRIPTION_TEXT_STYLE,
                        useDarkColors ? R.style.TextAppearance_TextMedium_Secondary_Dark
                                      : R.style.TextAppearance_TextMedium_Secondary_Light)
                .build();
    }

    private void handleItemClick(@Nullable GURL url, int resultPosition, boolean isProviderLabel) {
        // When the provider label is clicked, we should go back to the page where CSN started on.
        if (isProviderLabel) {
            if (mStartNavigationIndex >= 0 && mCurrentTab != null
                    && mCurrentTab.getWebContents() != null) {
                NavigationController navigationController =
                        mCurrentTab.getWebContents().getNavigationController();
                if (navigationController != null
                        && navigationController.getEntryAtIndex(mStartNavigationIndex) != null) {
                    navigationController.goToNavigationIndex(mStartNavigationIndex);
                }
            }
        } else if (mCurrentTab != null && url != null) {
            LoadUrlParams params = new LoadUrlParams(url.getSpec());
            params.setReferrer(
                    new Referrer("https://www.google.com", ReferrerPolicy.STRICT_ORIGIN));
            mCurrentTab.loadUrl(params);

            RecordHistogram.recordCount100Histogram(
                    "Browser.ContinuousSearch.UI.ClickedItemPosition"
                            + SearchUrlHelper.getHistogramSuffixForPageCategory(mPageCategory),
                    resultPosition);
        }
    }

    private void setVisibility(boolean visibility) {
        if (mVisible && !visibility) recordListScrolled();
        mVisible = visibility;
        mSetLayoutVisibility.onResult(mVisible);
    }

    void onScrolled() {
        mScrolled = true;
    }

    private void recordListScrolled() {
        RecordHistogram.recordBooleanHistogram("Browser.ContinuousSearch.UI.CarouselScrolled"
                        + SearchUrlHelper.getHistogramSuffixForPageCategory(mPageCategory),
                mScrolled);
        mScrolled = false;
    }

    @Override
    public void onThemeColorChanged(int color, boolean shouldAnimate) {
        // TODO(crbug.com/1192781): Animate the color change if necessary.
        mRootViewModel.set(ContinuousSearchListProperties.BACKGROUND_COLOR, color);
        mRootViewModel.set(ContinuousSearchListProperties.FOREGROUND_COLOR,
                shouldUseDarkElementColors(color) ? getColor(R.color.default_icon_color_dark)
                                                  : getColor(R.color.default_icon_color_light));

        int itemBgColor = getBackgroundColorForParentBackgroundColor(color);
        boolean useDarkColors = shouldUseDarkElementColors(itemBgColor);
        for (ListItem listItem : mModelList) {
            listItem.model.set(ContinuousSearchListProperties.BACKGROUND_COLOR, itemBgColor);
            listItem.model.set(ContinuousSearchListProperties.TITLE_TEXT_STYLE,
                    useDarkColors ? R.style.TextAppearance_TextMedium_Primary_Dark
                                  : R.style.TextAppearance_TextMedium_Primary_Light);
            listItem.model.set(ContinuousSearchListProperties.DESCRIPTION_TEXT_STYLE,
                    useDarkColors ? R.style.TextAppearance_TextMedium_Secondary_Dark
                                  : R.style.TextAppearance_TextMedium_Secondary_Light);
            listItem.model.set(ContinuousSearchListProperties.BORDER_COLOR,
                    useDarkColors ? getColor(R.color.default_icon_color_dark)
                                  : getColor(R.color.default_icon_color_light));
        }
    }

    private int getBackgroundColorForParentBackgroundColor(int parentColor) {
        // TODO(crbug.com/1192784): Pass isIncognito here.
        return ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                mResources, parentColor, false);
    }

    private boolean shouldUseDarkElementColors(int backgroundColor) {
        return !ColorUtils.shouldUseLightForegroundOnBackground(backgroundColor);
    }

    private int getColor(int id) {
        return ApiCompatibilityUtils.getColor(mResources, id);
    }

    void destroy() {
        if (mCurrentUserData != null) mCurrentUserData.removeObserver(this);
        if (mThemeColorProvider != null) mThemeColorProvider.removeThemeColorObserver(this);
    }
}
