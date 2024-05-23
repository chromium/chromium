// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.Size;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ClickInfo;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ModuleShowConfig;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallbacks;

/** The view containing suggestion tiles on the tab resumption module. */
public class TabResumptionTileContainerView extends LinearLayout {
    private final Size mThumbnailSize;

    public TabResumptionTileContainerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);

        int size =
                context.getResources()
                        .getDimensionPixelSize(
                                org.chromium.chrome.browser.tab_ui.R.dimen
                                        .single_tab_module_tab_thumbnail_size_big);
        mThumbnailSize = new Size(size, size);
    }

    @Override
    public void removeAllViews() {
        // Children alternate between tile and divider. Call destroy() on tiles.
        for (int i = 0; i < getChildCount(); ++i) {
            View view = getChildAt(i);
            if (view instanceof TabResumptionTileView) {
                ((TabResumptionTileView) view).destroy();
            }
        }
        super.removeAllViews();
    }

    public void destroy() {
        removeAllViews();
    }

    /**
     * Adds all new {@link TabResumptionTileView} instances, after removing existing ones and
     * returns the text of all instances.
     */
    public String renderAllTiles(
            SuggestionBundle bundle,
            UrlImageProvider urlImageProvider,
            SuggestionClickCallbacks suggestionClickCallbacks,
            boolean useSalientImage) {
        removeAllViews();

        @ModuleShowConfig
        Integer moduleShowConfig = TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle);

        String allTilesTexts = "";
        int entryCount = bundle.entries.size();
        boolean isSingle = entryCount == 1;
        int entryIndex = 0;
        for (SuggestionEntry entry : bundle.entries) {
            assert moduleShowConfig != null;
            @ClickInfo
            int clickInfo =
                    TabResumptionModuleMetricsUtils.computeClickInfo(
                            moduleShowConfig.intValue(), entryIndex);

            // Add divider if some tile already exists.
            if (getChildCount() > 0) {
                View divider =
                        (View)
                                LayoutInflater.from(getContext())
                                        .inflate(
                                                R.layout.tab_resumption_module_divider,
                                                this,
                                                false);
                addView(divider);
            }
            boolean isLocal = entry instanceof LocalTabSuggestionEntry;
            if (isLocal && isSingle) {
                allTilesTexts +=
                        loadLocalTabSingle(
                                        this,
                                        (LocalTabSuggestionEntry) entry,
                                        bundle.referenceTimeMs,
                                        urlImageProvider,
                                        suggestionClickCallbacks,
                                        clickInfo)
                                + ". ";
            } else {
                int layoutId =
                        isSingle
                                ? R.layout.tab_resumption_module_single_tile_layout
                                : R.layout.tab_resumption_module_multi_tile_layout;
                TabResumptionTileView tileView =
                        (TabResumptionTileView)
                                LayoutInflater.from(getContext()).inflate(layoutId, this, false);
                allTilesTexts +=
                        loadTileTexts(
                                        entry,
                                        bundle.referenceTimeMs,
                                        isSingle,
                                        tileView,
                                        useSalientImage)
                                + ". ";
                loadTileUrlImage(entry, urlImageProvider, tileView, isSingle, useSalientImage);
                bindSuggestionClickCallback(tileView, suggestionClickCallbacks, entry, clickInfo);
                addView(tileView);
            }
            ++entryIndex;
        }
        return allTilesTexts;
    }

    /** Renders and returns the texts of a {@link TabResumptionTileView}. */
    private String loadTileTexts(
            SuggestionEntry entry,
            long referenceTimeMs,
            boolean isSingle,
            TabResumptionTileView tileView,
            boolean useSalientImage) {
        Resources res = getContext().getResources();
        String recencyString =
                TabResumptionModuleUtils.getRecencyString(
                        getResources(), referenceTimeMs - entry.lastActiveTime);
        boolean isLocal = entry instanceof LocalTabSuggestionEntry;
        if (isSingle) {
            // Single Local Tab suggestion is handled by #loadLocalTabSingle().
            assert !isLocal;
            String preInfoText =
                    res.getString(R.string.tab_resumption_module_single_pre_info, entry.sourceName);
            String postInfoText =
                    res.getString(
                            R.string.tab_resumption_module_single_post_info,
                            recencyString,
                            TabResumptionModuleUtils.getDomainUrl(entry.url));
            tileView.setSuggestionTextsSingle(preInfoText, entry.title, postInfoText);
            return preInfoText + ", " + entry.title + ", " + postInfoText;
        }

        String infoText;
        String domainUrl = TabResumptionModuleUtils.getDomainUrl(entry.url);
        if (isLocal) {
            infoText =
                    useSalientImage
                            ? res.getString(
                                    R.string.tab_resumption_module_multi_info_local_with_url,
                                    domainUrl,
                                    recencyString)
                            : res.getString(
                                    R.string.tab_resumption_module_multi_info_local, recencyString);
        } else {
            infoText =
                    useSalientImage
                            ? res.getString(
                                    R.string.tab_resumption_module_multi_info_with_url,
                                    domainUrl,
                                    recencyString,
                                    entry.sourceName)
                            : res.getString(
                                    R.string.tab_resumption_module_multi_info,
                                    recencyString,
                                    entry.sourceName);
        }
        tileView.setSuggestionTextsMulti(entry.title, infoText);
        return entry.title + ", " + infoText;
    }

    /** Loads texts and images for the single Local Tab suggestion. */
    private String loadLocalTabSingle(
            ViewGroup parentView,
            LocalTabSuggestionEntry localTabEntry,
            long referenceTimeMs,
            UrlImageProvider urlImageProvider,
            SuggestionClickCallbacks suggestionClickCallback,
            @ClickInfo int clickInfo) {
        Tab tab = localTabEntry.tab;
        Resources res = getContext().getResources();
        String recencyString =
                TabResumptionModuleUtils.getRecencyString(
                        getResources(), referenceTimeMs - tab.getTimestampMillis());
        LocalTileView tileView =
                (LocalTileView)
                        LayoutInflater.from(getContext())
                                .inflate(
                                        R.layout.tab_resumption_module_local_tile_layout,
                                        this,
                                        false);
        String postInfoText =
                res.getString(
                        R.string.tab_resumption_module_single_post_info,
                        recencyString,
                        TabResumptionModuleUtils.getDomainUrl(tab.getUrl()));
        tileView.setUrl(postInfoText);
        tileView.setTitle(tab.getTitle());
        urlImageProvider.fetchImageForUrl(
                tab.getUrl(),
                (Bitmap bitmap) -> {
                    Drawable urlDrawable = new BitmapDrawable(res, bitmap);
                    tileView.setFavicon(urlDrawable);
                });
        urlImageProvider.getTabThumbnail(
                tab.getId(),
                mThumbnailSize,
                (Bitmap tabThumbnail) -> {
                    tileView.setTabThumbnail(tabThumbnail);
                });

        bindSuggestionClickCallback(tileView, suggestionClickCallback, localTabEntry, clickInfo);

        parentView.addView(tileView);
        return tab.getTitle() + ", " + postInfoText;
    }

    /** Loads the main URL image of a {@link TabResumptionTileView}. */
    @VisibleForTesting
    void loadTileUrlImage(
            SuggestionEntry entry,
            UrlImageProvider urlImageProvider,
            TabResumptionTileView tileView,
            boolean isSingle,
            boolean useSalientImage) {
        Runnable fetchRegularImage =
                () -> {
                    urlImageProvider.fetchImageForUrl(
                            entry.url,
                            (bitmap) -> {
                                onImageAvailable(
                                        bitmap,
                                        tileView,
                                        useSalientImage,
                                        /* isSalientImage= */ false);
                            });
                };
        if (useSalientImage) {
            Callback<Bitmap> fetchSalientImageCallback =
                    (bitmap) -> {
                        if (bitmap != null) {
                            onImageAvailable(
                                    bitmap,
                                    tileView,
                                    /* useSalientImage= */ true,
                                    /* isSalientImage= */ true);
                        } else {
                            fetchRegularImage.run();
                        }
                    };
            urlImageProvider.fetchSalientImage(entry.url, isSingle, fetchSalientImageCallback);
        } else {
            fetchRegularImage.run();
        }
    }

    /**
     * Called when the image bitmap is fetched.
     *
     * @param bitmap The image bitmap returned.
     * @param tileView The tile view to show the image.
     * @param useSalientImage Whether a salient image is requested.
     * @param isSalientImage Whether the returned image is a salient image.
     */
    private void onImageAvailable(
            Bitmap bitmap,
            TabResumptionTileView tileView,
            boolean useSalientImage,
            boolean isSalientImage) {
        Resources res = getContext().getResources();
        Drawable urlDrawable = new BitmapDrawable(res, bitmap);
        tileView.setImageDrawable(urlDrawable);
        if (isSalientImage) {
            tileView.updateForSalientImage();
        }
        if (useSalientImage) {
            TabResumptionModuleMetricsUtils.recordSalientImageAvailability(isSalientImage);
        }
    }

    /** Binds the click handler with an associated URL. */
    private void bindSuggestionClickCallback(
            View tileView,
            SuggestionClickCallbacks callbacks,
            SuggestionEntry entry,
            @ClickInfo int clickInfo) {
        tileView.setOnClickListener(
                v -> {
                    TabResumptionModuleMetricsUtils.recordClickInfo(clickInfo);
                    if (entry instanceof LocalTabSuggestionEntry) {
                        callbacks.onSuggestionClickByTabId(
                                ((LocalTabSuggestionEntry) entry).tab.getId());
                    } else {
                        callbacks.onSuggestionClickByUrl(entry.url);
                    }
                });
        // Handle and return false to avoid obstructing long click handling of containing Views.
        tileView.setOnLongClickListener(v -> false);
    }
}
