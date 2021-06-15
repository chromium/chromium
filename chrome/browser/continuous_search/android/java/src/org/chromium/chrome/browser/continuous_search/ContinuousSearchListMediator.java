// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.content.res.Resources;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchContainerCoordinator.VisibilitySettings;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ListItemProperties;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ListItemType;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ProviderProperties;
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
    private final Callback<VisibilitySettings> mSetLayoutVisibility;
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
            Callback<VisibilitySettings> setLayoutVisibility, ThemeColorProvider themeColorProvider,
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

        setVisibility(false, null);
        reset();
        mCurrentTab = tab;
        if (mCurrentTab == null) return;

        mCurrentUserData = ContinuousNavigationUserDataImpl.getOrCreateForTab(mCurrentTab);
        mCurrentUserData.addObserver(this);
    }

    @Override
    public void onInvalidate() {
        setVisibility(false, this::reset);
    }

    private void reset() {
        mModelList.clear();
        mOnSrp = false;
    }

    @Override
    public void onUpdate(ContinuousNavigationMetadata metadata) {
        mModelList.clear();

        ContinuousNavigationMetadata.Provider provider = metadata.getProvider();
        mPageCategory = provider.getCategory();
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

        mModelList.add(new ListItem(ListItemType.PROVIDER,
                generateProvider(provider.getName(), provider.getIconRes())));

        int resultCount = 0;
        for (PageGroup group : metadata.getGroups()) {
            int itemType = group.isAdGroup() ? ListItemType.AD : ListItemType.SEARCH_RESULT;
            for (PageItem result : group.getPageItems()) {
                mModelList.add(new ListItem(itemType,
                        generateListItem(result.getTitle(), result.getUrl(), resultCount++)));
            }
        }
    }

    @Override
    public void onUrlChanged(GURL currentUrl, boolean onSrp) {
        mOnSrp = onSrp;
        for (ListItem listItem : mModelList) {
            if (listItem.type == ListItemType.PROVIDER) continue;

            boolean isSelected = currentUrl != null
                    && currentUrl.equals(listItem.model.get(ListItemProperties.URL));
            listItem.model.set(ListItemProperties.IS_SELECTED, isSelected);
        }
        setVisibility(mModelList.size() > 0 && !mOnSrp, null);
    }

    /**
     * Generates the {@link PropertyModel} for the provider.
     * @param label     Provider label text. Can be null.
     * @param iconRes   Provider icon resource. Pass 0 here if there is no icon.
     * @return the configured {@link PropertyModel}.
     */
    private PropertyModel generateProvider(String label, @DrawableRes int iconRes) {
        int backgroundColor =
                getBackgroundColorForParentBackgroundColor(mThemeColorProvider.getThemeColor());
        boolean useDarkColors = shouldUseDarkElementColors(backgroundColor);
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ProviderProperties.ALL_KEYS)
                        .with(ProviderProperties.CLICK_LISTENER,
                                (view)
                                        -> handleItemClick(/*url=*/null, /*resultPosition=*/0,
                                                /*isProviderLabel=*/true))
                        .with(ProviderProperties.TEXT_STYLE,
                                useDarkColors ? R.style.TextAppearance_TextMedium_Primary_Dark
                                              : R.style.TextAppearance_TextMedium_Primary_Light);
        if (label != null) {
            builder = builder.with(ProviderProperties.LABEL,
                    mResources.getString(R.string.csn_provider_label, label));
        }
        if (iconRes != 0) builder = builder.with(ProviderProperties.ICON_RESOURCE, iconRes);
        return builder.build();
    }

    /**
     * Generates a list item with the given attributes.
     * @param text            Displayed as the primary text.
     * @param url             Displayed as teh secondary text.
     * @param resultPosition  Denotes the position of this result in the list.
     * @return {@link PropertyModel} representing this item.
     */
    private PropertyModel generateListItem(String text, GURL url, int resultPosition) {
        int backgroundColor =
                getBackgroundColorForParentBackgroundColor(mThemeColorProvider.getThemeColor());
        boolean useDarkColors = shouldUseDarkElementColors(backgroundColor);
        return new PropertyModel.Builder(ListItemProperties.ALL_KEYS)
                .with(ListItemProperties.LABEL, text)
                .with(ListItemProperties.URL, url)
                .with(ListItemProperties.IS_SELECTED, false)
                .with(ListItemProperties.BORDER_COLOR,
                        useDarkColors ? getColor(R.color.default_icon_color_dark)
                                      : getColor(R.color.default_icon_color_light))
                .with(ListItemProperties.CLICK_LISTENER,
                        (view) -> handleItemClick(url, resultPosition, /*isProviderLabel=*/false))
                .with(ListItemProperties.BACKGROUND_COLOR, backgroundColor)
                .with(ListItemProperties.TITLE_TEXT_STYLE,
                        useDarkColors ? R.style.TextAppearance_TextMedium_Primary_Dark
                                      : R.style.TextAppearance_TextMedium_Primary_Light)
                .with(ListItemProperties.DESCRIPTION_TEXT_STYLE,
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

    private void setVisibility(boolean visibility, Runnable onHideFinished) {
        if (mVisible && !visibility) recordListScrolled();
        mVisible = visibility;
        mSetLayoutVisibility.onResult(new VisibilitySettings(mVisible, onHideFinished));
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
            if (listItem.type == ListItemType.PROVIDER) {
                listItem.model.set(ProviderProperties.TEXT_STYLE,
                        useDarkColors ? R.style.TextAppearance_TextMedium_Primary_Dark
                                      : R.style.TextAppearance_TextMedium_Primary_Light);
            } else {
                listItem.model.set(ListItemProperties.BACKGROUND_COLOR, itemBgColor);
                listItem.model.set(ListItemProperties.TITLE_TEXT_STYLE,
                        useDarkColors ? R.style.TextAppearance_TextMedium_Primary_Dark
                                      : R.style.TextAppearance_TextMedium_Primary_Light);
                listItem.model.set(ListItemProperties.DESCRIPTION_TEXT_STYLE,
                        useDarkColors ? R.style.TextAppearance_TextMedium_Secondary_Dark
                                      : R.style.TextAppearance_TextMedium_Secondary_Light);
                listItem.model.set(ListItemProperties.BORDER_COLOR,
                        useDarkColors ? getColor(R.color.default_icon_color_dark)
                                      : getColor(R.color.default_icon_color_light));
            }
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
