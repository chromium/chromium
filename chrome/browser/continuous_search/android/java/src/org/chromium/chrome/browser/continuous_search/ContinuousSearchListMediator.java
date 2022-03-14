// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.continuous_search;

import android.content.Context;

import androidx.annotation.DrawableRes;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchContainerCoordinator.VisibilitySettings;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ListItemProperties;
import org.chromium.chrome.browser.continuous_search.ContinuousSearchListProperties.ListItemType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Business logic for the UI component of Continuous Search Navigation. This class updates the UI on
 * search result updates.
 */
class ContinuousSearchListMediator implements ContinuousNavigationUserDataObserver, Callback<Tab>,
                                              ThemeColorProvider.ThemeColorObserver {
    @VisibleForTesting
    static final String TRIGGER_MODE_PARAM = "trigger_mode";
    static final String SHOW_RESULT_TITLE_PARAM = "show_result_title";

    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final ModelList mModelList;
    private final PropertyModel mRootViewModel;
    private final Callback<VisibilitySettings> mSetLayoutVisibility;
    private final ThemeColorProvider mThemeColorProvider;
    private final Context mContext;
    private Tab mCurrentTab;
    private boolean mOnSrp;
    private ContinuousNavigationUserDataImpl mCurrentUserData;
    private @PageCategory int mPageCategory;
    private boolean mVisible;
    private boolean mDismissed;
    // The navigation index when CSN metadata was retrieved.
    private int mStartNavigationIndex;
    private int mSrpVisits;
    private BrowserControlsStateProvider.Observer mScrollObserver;

    @VisibleForTesting
    boolean mScrolled;

    @IntDef({TriggerMode.ALWAYS, TriggerMode.AFTER_SECOND_SRP, TriggerMode.ON_REVERSE_SCROLL})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TriggerMode {
        int ALWAYS = 0;
        int AFTER_SECOND_SRP = 1;
        int ON_REVERSE_SCROLL = 2;
    };

    ContinuousSearchListMediator(BrowserControlsStateProvider browserControlsStateProvider,
            ModelList modelList, PropertyModel rootViewModel,
            Callback<VisibilitySettings> setLayoutVisibility, ThemeColorProvider themeColorProvider,
            Context context) {
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mModelList = modelList;
        mRootViewModel = rootViewModel;
        mSetLayoutVisibility = setLayoutVisibility;
        mThemeColorProvider = themeColorProvider;
        mContext = context;

        mRootViewModel.set(ContinuousSearchListProperties.DISMISS_CLICK_CALLBACK,
                (v) -> dismissOnUserRequest());
        if (mThemeColorProvider != null) {
            mThemeColorProvider.addThemeColorObserver(this);
            int themeColor = mThemeColorProvider.getThemeColor();
            mRootViewModel.set(ContinuousSearchListProperties.BACKGROUND_COLOR, themeColor);
            mRootViewModel.set(ContinuousSearchListProperties.FOREGROUND_COLOR,
                    shouldUseDarkElementColors(themeColor)
                            ? getColor(R.color.default_icon_color_dark)
                            : getColor(R.color.default_icon_color_light));
        }
        initScrollObserver();
    }

    private void dismissOnUserRequest() {
        TraceEvent.begin("ContinuousSearchListMediator#dismissOnUserRequest");
        // To avoid showing for duration of the current SRP session don't delete the data, instead
        // hide the UI permamently. Data will be deleted as soon as the SRP session is over.
        mDismissed = true;
        ContinuousSearchConfiguration.recordDismissed();
        setVisibility(false, null);
        TraceEvent.end("ContinuousSearchListMediator#dismissOnUserRequest");
    }

    private void reset() {
        mModelList.clear();
        mDismissed = false;
        mOnSrp = false;
        mScrolled = false;
        mSrpVisits = 0;
    }

    private boolean shouldShow() {
        return mModelList.size() > 0 && !mOnSrp && !mDismissed;
    }

    /**
     * Called on observing a new tab.
     */
    @Override
    public void onResult(Tab tab) {
        TraceEvent.begin("ContinuousSearchListMediator#onResult");
        if (mCurrentUserData != null) {
            mCurrentUserData.removeObserver(this);
            mCurrentUserData = null;
        }

        if (mScrollObserver != null) {
            mBrowserControlsStateProvider.removeObserver(mScrollObserver);
        }

        setVisibility(false, null);
        reset();
        mCurrentTab = tab;
        if (mCurrentTab == null) {
            TraceEvent.end("ContinuousSearchListMediator#onResult");
            return;
        }

        if (mScrollObserver != null) {
            mBrowserControlsStateProvider.addObserver(mScrollObserver);
        }
        mCurrentUserData = ContinuousNavigationUserDataImpl.getOrCreateForTab(mCurrentTab);
        mCurrentUserData.addObserver(this);
        TraceEvent.end("ContinuousSearchListMediator#onResult");
    }

    @Override
    public void onInvalidate() {
        setVisibility(false, this::reset);
    }

    @Override
    public void onUpdate(ContinuousNavigationMetadata metadata) {
        TraceEvent.begin("ContinuousSearchListMediator#onUpdate");
        reset();

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

        setProviderProperties(provider.getName(), provider.getIconRes());

        int resultCount = 0;
        for (PageGroup group : metadata.getGroups()) {
            int itemType = group.isAdGroup() ? ListItemType.AD : ListItemType.SEARCH_RESULT;
            for (PageItem result : group.getPageItems()) {
                mModelList.add(new ListItem(itemType,
                        generateListItem(result.getTitle(), result.getUrl(), resultCount++)));
            }
        }
        TraceEvent.end("ContinuousSearchListMediator#onUpdate");
    }

    @Override
    public void onUrlChanged(GURL currentUrl, boolean onSrp) {
        TraceEvent.begin("ContinuousSearchListMediator#onUrlChanged");
        mOnSrp = onSrp;
        if (mOnSrp) mSrpVisits++;

        int selectedItemPosition = -1;
        for (int i = 0; i < mModelList.size(); i++) {
            ListItem listItem = mModelList.get(i);

            boolean isSelected = currentUrl != null
                    && currentUrl.equals(listItem.model.get(ListItemProperties.URL));
            listItem.model.set(ListItemProperties.IS_SELECTED, isSelected);
            if (isSelected) selectedItemPosition = i;
        }

        boolean shouldTrigger = false;
        switch (getTriggerMode()) {
            case TriggerMode.ALWAYS:
                shouldTrigger = true;
                break;
            case TriggerMode.AFTER_SECOND_SRP:
                shouldTrigger = mSrpVisits >= 2;
                break;
            case TriggerMode.ON_REVERSE_SCROLL:
                // Keep showing once open as the UI was shown and can be used.
                shouldTrigger = mVisible;
                break;
        }
        boolean shouldBeVisible = shouldShow() && shouldTrigger;
        Runnable onFinishShowRunnable = null;
        if (selectedItemPosition != -1) {
            final int finalSelectedItemPosition = selectedItemPosition;
            onFinishShowRunnable = ()
                    -> mRootViewModel.set(ContinuousSearchListProperties.SELECTED_ITEM_POSITION,
                            finalSelectedItemPosition);
        }
        setVisibility(shouldBeVisible, onFinishShowRunnable);
        TraceEvent.end("ContinuousSearchListMediator#onUrlChanged");
    }

    private @TriggerMode int getTriggerMode() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.CONTINUOUS_SEARCH, TRIGGER_MODE_PARAM, TriggerMode.ALWAYS);
    }

    boolean shouldShowResultTitle() {
        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CONTINUOUS_SEARCH, SHOW_RESULT_TITLE_PARAM, false);
    }

    /**
     * Sets the provider properties on the root PropertyModel.
     * @param label     Provider label text. Can be null.
     * @param iconRes   Provider icon resource. Pass 0 here if there is no icon.
     */
    private void setProviderProperties(String label, @DrawableRes int iconRes) {
        int backgroundColor =
                getBackgroundColorForParentBackgroundColor(mThemeColorProvider.getThemeColor());
        boolean useDarkColors = shouldUseDarkElementColors(backgroundColor);
        mRootViewModel.set(ContinuousSearchListProperties.PROVIDER_CLICK_LISTENER,
                (view)
                        -> handleItemClick(/*url=*/null, /*resultPosition=*/0,
                                /*isProviderLabel=*/true));
        mRootViewModel.set(ContinuousSearchListProperties.PROVIDER_TEXT_STYLE,
                useDarkColors ? R.style.TextAppearance_TextMedium_Primary_Baseline_Dark
                              : R.style.TextAppearance_TextMedium_Primary_Baseline_Light);
        if (label != null) {
            mRootViewModel.set(ContinuousSearchListProperties.PROVIDER_LABEL,
                    mContext.getString(R.string.csn_provider_label, label));
        }
        if (iconRes != 0) {
            mRootViewModel.set(ContinuousSearchListProperties.PROVIDER_ICON_RESOURCE, iconRes);
        }
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
                .with(ListItemProperties.PRIMARY_TEXT_STYLE,
                        useDarkColors ? R.style.TextAppearance_ContinuousNavigationChipText_Dark
                                      : R.style.TextAppearance_ContinuousNavigationChipText_Light)
                .with(ListItemProperties.SECONDARY_TEXT_STYLE,
                        useDarkColors ? R.style.TextAppearance_ContinuousNavigationChipHint_Dark
                                      : R.style.TextAppearance_ContinuousNavigationChipHint_Light)
                .build();
    }

    private void handleItemClick(@Nullable GURL url, int resultPosition, boolean isProviderLabel) {
        TraceEvent.begin("ContinuousSearchListMediator#handleItemClick");
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
        }
        TraceEvent.end("ContinuousSearchListMediator#handleItemClick");
    }

    private void setVisibility(boolean visibility, Runnable onFinished) {
        TraceEvent.begin("ContinuousSearchListMediator#setVisibility");
        mVisible = visibility;
        mSetLayoutVisibility.onResult(new VisibilitySettings(mVisible, onFinished));
        TraceEvent.end("ContinuousSearchListMediator#setVisibility");
    }

    void onScrolled() {
        mScrolled = true;
    }

    private void initScrollObserver() {
        if (getTriggerMode() != TriggerMode.ON_REVERSE_SCROLL) return;

        mScrollObserver = new BrowserControlsStateProvider.Observer() {
            @Override
            public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
                    int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
                if (mVisible) return;

                if (!shouldShow()) return;

                // Show the UI only when the browser controls are fully hidden then on any
                // subsequent reverse scroll the omnibox will be shown along with the UI.
                if (BrowserControlsUtils.areBrowserControlsOffScreen(
                            mBrowserControlsStateProvider)) {
                    setVisibility(true, null);
                }
            }
        };
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
        mRootViewModel.set(ContinuousSearchListProperties.PROVIDER_TEXT_STYLE,
                useDarkColors ? R.style.TextAppearance_TextMedium_Primary_Baseline_Dark
                              : R.style.TextAppearance_TextMedium_Primary_Baseline_Light);
        for (ListItem listItem : mModelList) {
            listItem.model.set(ListItemProperties.BACKGROUND_COLOR, itemBgColor);
            listItem.model.set(ListItemProperties.PRIMARY_TEXT_STYLE,
                    useDarkColors ? R.style.TextAppearance_ContinuousNavigationChipText_Dark
                                  : R.style.TextAppearance_ContinuousNavigationChipText_Light);
            listItem.model.set(ListItemProperties.SECONDARY_TEXT_STYLE,
                    useDarkColors ? R.style.TextAppearance_ContinuousNavigationChipHint_Dark
                                  : R.style.TextAppearance_ContinuousNavigationChipHint_Light);
            listItem.model.set(ListItemProperties.BORDER_COLOR,
                    useDarkColors ? getColor(R.color.default_icon_color_dark)
                                  : getColor(R.color.default_icon_color_light));
        }
    }

    private int getBackgroundColorForParentBackgroundColor(int parentColor) {
        // TODO(crbug.com/1192784): Pass isIncognito here.
        return ThemeUtils.getTextBoxColorForToolbarBackgroundInNonNativePage(
                mContext, parentColor, false);
    }

    private boolean shouldUseDarkElementColors(int backgroundColor) {
        return !ColorUtils.shouldUseLightForegroundOnBackground(backgroundColor);
    }

    private int getColor(int id) {
        return mContext.getColor(id);
    }

    void destroy() {
        reset();

        if (mCurrentUserData != null) mCurrentUserData.removeObserver(this);
        if (mThemeColorProvider != null) mThemeColorProvider.removeThemeColorObserver(this);

        if (mScrollObserver != null) {
            mBrowserControlsStateProvider.removeObserver(mScrollObserver);
        }
    }
}
