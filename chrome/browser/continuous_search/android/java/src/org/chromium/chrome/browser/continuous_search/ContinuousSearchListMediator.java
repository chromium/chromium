// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.content.res.Resources;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ListItemType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ThemeUtils;
import org.chromium.content_public.browser.LoadUrlParams;
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
class ContinuousSearchListMediator implements ContinuousNavigationUserDataObserver, Callback<Tab> {
    private final ModelList mModelList;
    private final Callback<Boolean> mSetLayoutVisibility;
    private final ThemeColorProvider mThemeColorProvider;
    private final Resources mResources;
    private Tab mCurrentTab;
    private boolean mOnSrp;
    private ContinuousNavigationUserDataImpl mCurrentUserData;
    private @PageCategory int mPageCategory;
    private boolean mVisible;
    private boolean mScrolled;

    ContinuousSearchListMediator(ModelList modelList, Callback<Boolean> setLayoutVisibility,
            ThemeColorProvider themeColorProvider, Resources resources) {
        mModelList = modelList;
        mSetLayoutVisibility = setLayoutVisibility;
        mThemeColorProvider = themeColorProvider;
        mResources = resources;
    }

    private void handleResultClick(GURL url, int position) {
        if (url == null || mCurrentTab == null) return;

        LoadUrlParams params = new LoadUrlParams(url.getSpec());
        params.setReferrer(new Referrer("https://www.google.com", ReferrerPolicy.STRICT_ORIGIN));
        mCurrentTab.loadUrl(params);

        RecordHistogram.recordCount100Histogram("Browser.ContinuousSearch.UI.ClickedItemPosition"
                        + SearchUrlHelper.getHistogramSuffixForPageCategory(mPageCategory),
                position);
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

        int linkCount = 0;
        for (PageGroup group : metadata.getGroups()) {
            if (!group.isAdGroup()) {
                mModelList.add(new ListItem(
                        ListItemType.GROUP_LABEL, generateListItem(group.getLabel(), null, 0)));
            }
            int itemType = group.isAdGroup() ? ListItemType.AD : ListItemType.SEARCH_RESULT;
            for (PageItem result : group.getPageItems()) {
                mModelList.add(new ListItem(itemType,
                        generateListItem(result.getTitle(), result.getUrl(), linkCount++)));
            }
        }
        mPageCategory = metadata.getCategory();
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

    private PropertyModel generateListItem(String text, GURL url, int position) {
        int backgroundColor =
                getBackgroundColorForParentBackgroundColor(mThemeColorProvider.getThemeColor());
        boolean useDarkTextColors = shouldUseDarkTextColors(backgroundColor);
        return new PropertyModel.Builder(ContinuousSearchListProperties.ALL_KEYS)
                .with(ContinuousSearchListProperties.LABEL, text)
                .with(ContinuousSearchListProperties.URL, url)
                .with(ContinuousSearchListProperties.IS_SELECTED, false)
                .with(ContinuousSearchListProperties.CLICK_LISTENER,
                        (view) -> handleResultClick(url, position))
                .with(ContinuousSearchListProperties.BACKGROUND_COLOR, backgroundColor)
                .with(ContinuousSearchListProperties.TITLE_TEXT_STYLE,
                        useDarkTextColors ? R.style.TextAppearance_TextMedium_Primary_Dark
                                          : R.style.TextAppearance_TextMedium_Primary_Light)
                .with(ContinuousSearchListProperties.DESCRIPTION_TEXT_STYLE,
                        useDarkTextColors ? R.style.TextAppearance_TextMedium_Secondary_Dark
                                          : R.style.TextAppearance_TextMedium_Secondary_Light)
                .build();
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

    void destroy() {
        if (mCurrentUserData != null) mCurrentUserData.removeObserver(this);
    }

    void onThemeColorChanged(int color, boolean shouldAnimate) {
        // TODO(crbug.com/1192781): Animate the color change if necessary.
        int bgColor = getBackgroundColorForParentBackgroundColor(color);
        boolean useDarkTextColors = shouldUseDarkTextColors(bgColor);
        for (ListItem listItem : mModelList) {
            listItem.model.set(ContinuousSearchListProperties.BACKGROUND_COLOR, bgColor);
            listItem.model.set(ContinuousSearchListProperties.TITLE_TEXT_STYLE,
                    useDarkTextColors ? R.style.TextAppearance_TextMedium_Primary_Dark
                                      : R.style.TextAppearance_TextMedium_Primary_Light);
            listItem.model.set(ContinuousSearchListProperties.DESCRIPTION_TEXT_STYLE,
                    useDarkTextColors ? R.style.TextAppearance_TextMedium_Secondary_Dark
                                      : R.style.TextAppearance_TextMedium_Secondary_Light);
        }
    }

    private int getBackgroundColorForParentBackgroundColor(int parentColor) {
        // TODO(crbug.com/1192784): Pass isIncognito here.
        return ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                mResources, parentColor, false);
    }

    private boolean shouldUseDarkTextColors(int backgroundColor) {
        return !ColorUtils.shouldUseLightForegroundOnBackground(backgroundColor);
    }
}
