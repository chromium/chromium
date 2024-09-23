// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ModuleShowConfig;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * The View for the tab resumption module, consisting of a header followed by suggestion tile(s).
 */
public class TabResumptionModuleView extends LinearLayout {
    private TabResumptionTileContainerView mTileContainerView;
    private UrlImageProvider mUrlImageProvider;
    private SuggestionClickCallback mClickCallback;
    private SuggestionBundle mBundle;
    private boolean mUseSalientImage;

    private boolean mIsSuggestionBundleReady;
    private String mTitle;
    private String mSeeMoreViewText;
    private String mAllTilesTexts;
    private ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private Callback<Tab> mTabObserverCallback;
    @Nullable private Callback<Integer> mOnModuleShowConfigFinalizedCallback;
    @Nullable private Tab mTrackingTab;

    public TabResumptionModuleView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        mUseSalientImage = TabResumptionModuleUtils.TAB_RESUMPTION_USE_SALIENT_IMAGE.getValue();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTileContainerView = findViewById(R.id.tab_resumption_module_tiles_container);
        Resources res = getContext().getResources();
        mSeeMoreViewText = res.getString(R.string.tab_resumption_module_see_more);
        ((TextView) findViewById(R.id.tab_resumption_see_more_link))
                .setVisibility(
                        TabResumptionModuleUtils.TAB_RESUMPTION_SHOW_SEE_MORE.getValue()
                                ? View.VISIBLE
                                : View.GONE);
    }

    void destroy() {
        mTileContainerView.destroy();
    }

    void setUrlImageProvider(UrlImageProvider urlImageProvider) {
        mUrlImageProvider = urlImageProvider;
        renderIfReady();
    }

    void setSeeMoreLinkClickCallback(Runnable seeMoreClickCallback) {
        ((TextView) findViewById(R.id.tab_resumption_see_more_link))
                .setOnClickListener(
                        v -> {
                            seeMoreClickCallback.run();
                            @ModuleShowConfig
                            int config =
                                    TabResumptionModuleMetricsUtils.computeModuleShowConfig(
                                            mBundle);
                            TabResumptionModuleMetricsUtils.recordSeeMoreLinkClicked(config);
                        });
    }

    void setClickCallback(SuggestionClickCallback clickCallback) {
        mClickCallback = clickCallback;
        renderIfReady();
    }

    /** Assumes `mUrlImageProvider` and `mClickCallback` are assigned, triggers render. */
    void setSuggestionBundle(SuggestionBundle bundle) {
        mIsSuggestionBundleReady = true;
        mBundle = bundle;
        renderIfReady();
    }

    void setTitle(@Nullable String title) {
        mTitle = title;
        ((TextView) findViewById(R.id.tab_resumption_title_description)).setText(mTitle);
        setContentDescriptionOfTabResumption();
    }

    void setTabModelSelectorSupplier(
            @NonNull ObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        renderIfReady();
    }

    /** Sets the Tab that is tracked by the module's host surface. */
    void setTrackingTab(@Nullable Tab tab) {
        mTrackingTab = tab;
    }

    void setTabObserverCallback(@NonNull Callback<Tab> callback) {
        mTabObserverCallback = callback;
        renderIfReady();
    }

    void setOnModuleShowConfigFinalizedCallback(Callback<Integer> callback) {
        mOnModuleShowConfigFinalizedCallback = callback;
        renderIfReady();
    }

    TabResumptionTileContainerView getTileContainerViewForTesting() {
        return mTileContainerView;
    }

    private void renderIfReady() {
        if (mIsSuggestionBundleReady
                && mUrlImageProvider != null
                && mClickCallback != null
                && mTabModelSelectorSupplier != null
                && mTabObserverCallback != null
                && (TabResumptionModuleUtils.areSuggestionsFinalized(mBundle)
                        || mOnModuleShowConfigFinalizedCallback != null)) {
            if (mBundle == null) {
                mTileContainerView.removeAllViews();
                mTileContainerView.cancelAllCallbacks();
                mAllTilesTexts = null;
            } else {
                assert mTabModelSelectorSupplier.hasValue();
                mAllTilesTexts =
                        mTileContainerView.renderAllTiles(
                                mBundle,
                                mUrlImageProvider,
                                mClickCallback,
                                mUseSalientImage,
                                mTabModelSelectorSupplier.get(),
                                mTrackingTab,
                                mTabObserverCallback,
                                mOnModuleShowConfigFinalizedCallback);
            }
            setContentDescriptionOfTabResumption();
        }
    }

    /** Sets the content description for the tab resumption module. */
    private void setContentDescriptionOfTabResumption() {
        if (mTitle != null && mSeeMoreViewText != null && mAllTilesTexts != null) {
            setContentDescription(mTitle + ". " + mSeeMoreViewText + ". " + mAllTilesTexts);
        } else {
            setContentDescription(null);
        }
    }
}
