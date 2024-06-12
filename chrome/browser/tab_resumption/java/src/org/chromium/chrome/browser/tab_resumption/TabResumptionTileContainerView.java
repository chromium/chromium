// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.Context;
import android.content.pm.PackageManager;
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
import org.chromium.base.CallbackController;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ClickInfo;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ModuleShowConfig;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;

/** The view containing suggestion tiles on the tab resumption module. */
public class TabResumptionTileContainerView extends LinearLayout {
    private final Size mThumbnailSize;
    private PackageManager mPackageManager;

    // The mCallbackController could be null if not used or when all tiles are removed.
    @Nullable private CallbackController mCallbackController;

    public TabResumptionTileContainerView(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);

        int size =
                context.getResources()
                        .getDimensionPixelSize(
                                org.chromium.chrome.browser.tab_ui.R.dimen
                                        .single_tab_module_tab_thumbnail_size_big);
        mThumbnailSize = new Size(size, size);
        mPackageManager = context.getPackageManager();
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
        reset();
    }

    public void cancelAllCallbacks() {
        if (mCallbackController == null) return;

        mCallbackController.destroy();
        mCallbackController = null;
    }

    @VisibleForTesting
    void reset() {
        removeAllViews();
        cancelAllCallbacks();
    }

    /**
     * Adds all new {@link TabResumptionTileView} instances, after removing existing ones and
     * returns the text of all instances.
     */
    public String renderAllTiles(
            SuggestionBundle bundle,
            UrlImageProvider urlImageProvider,
            SuggestionClickCallback suggestionClickCallback,
            boolean useSalientImage) {
        reset();

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

            long recencyMs = bundle.referenceTimeMs - entry.lastActiveTime;
            TabResumptionModuleMetricsUtils.recordTabRecencyShow(recencyMs);
            SuggestionClickCallback suggestionClickCallbackWithLogging =
                    (SuggestionEntry clickedEntry) -> {
                        TabResumptionModuleMetricsUtils.recordTabRecencyClick(recencyMs);
                        suggestionClickCallback.onSuggestionClicked(clickedEntry);
                    };

            if (entry.isLocalTab() && isSingle) {
                allTilesTexts +=
                        loadLocalTabSingle(
                                this,
                                entry,
                                recencyMs,
                                urlImageProvider,
                                suggestionClickCallbackWithLogging,
                                clickInfo);
            } else {
                int layoutId =
                        isSingle
                                ? R.layout.tab_resumption_module_single_tile_layout
                                : R.layout.tab_resumption_module_multi_tile_layout;
                TabResumptionTileView tileView =
                        (TabResumptionTileView)
                                LayoutInflater.from(getContext()).inflate(layoutId, this, false);
                allTilesTexts +=
                        loadTileTexts(entry, recencyMs, isSingle, tileView, useSalientImage);
                loadTileUrlImage(
                        entry,
                        urlImageProvider,
                        tileView,
                        isSingle,
                        entry.type != SuggestionEntryType.HISTORY ? useSalientImage : false);
                bindSuggestionClickCallback(
                        tileView, suggestionClickCallbackWithLogging, entry, clickInfo);
                addView(tileView);
            }
            ++entryIndex;
        }
        return allTilesTexts;
    }

    /** Renders and returns the texts of a {@link TabResumptionTileView}. */
    private String loadTileTexts(
            SuggestionEntry entry,
            long recencyMs,
            boolean isSingle,
            TabResumptionTileView tileView,
            boolean useSalientImage) {
        Resources res = getContext().getResources();
        String domainUrl = TabResumptionModuleUtils.getDomainUrl(entry.url);
        String recencyString = TabResumptionModuleUtils.getRecencyString(getResources(), recencyMs);
        if (isSingle) {
            // Single Local Tab suggestion is handled by #loadLocalTabSingle().
            assert !entry.isLocalTab();
            String appChipText =
                    tileView.maybeShowAppChip(mPackageManager, entry.type, entry.appId);
            String postInfoText =
                    entry.type == SuggestionEntryType.HISTORY
                            ? res.getString(
                                    R.string.tab_resumption_module_single_post_info,
                                    domainUrl,
                                    recencyString)
                            : res.getString(
                                    R.string.tab_resumption_module_multi_info_with_url,
                                    domainUrl,
                                    entry.sourceName,
                                    recencyString);
            return tileView.setSuggestionTextsSingle(null, appChipText, entry.title, postInfoText);
        }

        String infoText;
        if (entry.isLocalTab()) {
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
                                    entry.sourceName,
                                    recencyString)
                            : res.getString(
                                    R.string.tab_resumption_module_multi_info,
                                    entry.sourceName,
                                    recencyString);
        }
        return tileView.setSuggestionTextsMulti(entry.title, infoText);
    }

    /** Loads texts and images for the single Local Tab suggestion. */
    private String loadLocalTabSingle(
            ViewGroup parentView,
            SuggestionEntry localTabEntry,
            long recencyMs,
            UrlImageProvider urlImageProvider,
            SuggestionClickCallback suggestionClickCallback,
            @ClickInfo int clickInfo) {
        assert localTabEntry.isLocalTab();
        Resources res = getContext().getResources();
        String recencyString = TabResumptionModuleUtils.getRecencyString(getResources(), recencyMs);
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
                        TabResumptionModuleUtils.getDomainUrl(localTabEntry.url),
                        recencyString);
        tileView.setUrl(postInfoText);
        tileView.setTitle(localTabEntry.title);
        urlImageProvider.fetchImageForUrl(
                localTabEntry.url,
                (Bitmap bitmap) -> {
                    Drawable urlDrawable = new BitmapDrawable(res, bitmap);
                    tileView.setFavicon(urlDrawable);
                });
        urlImageProvider.getTabThumbnail(
                localTabEntry.localTabId,
                mThumbnailSize,
                (Bitmap tabThumbnail) -> {
                    tileView.setTabThumbnail(tabThumbnail);
                });

        bindSuggestionClickCallback(tileView, suggestionClickCallback, localTabEntry, clickInfo);

        parentView.addView(tileView);
        return localTabEntry.title
                + TabResumptionTileView.SEPARATE_COMMA
                + postInfoText
                + TabResumptionTileView.SEPARATE_PERIOD;
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
                getCallbackController()
                        .makeCancelable(
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
                                });
        if (useSalientImage) {
            Callback<Bitmap> fetchSalientImageCallback =
                    getCallbackController()
                            .makeCancelable(
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
                                    });
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
            SuggestionClickCallback callback,
            SuggestionEntry entry,
            @ClickInfo int clickInfo) {
        tileView.setOnClickListener(
                v -> {
                    TabResumptionModuleMetricsUtils.recordClickInfo(clickInfo);
                    callback.onSuggestionClicked(entry);
                });
        // Handle and return false to avoid obstructing long click handling of containing Views.
        tileView.setOnLongClickListener(v -> false);
    }

    private CallbackController getCallbackController() {
        if (mCallbackController == null) {
            mCallbackController = new CallbackController();
        }
        return mCallbackController;
    }

    void setPackageManagerForTesting(PackageManager pm) {
        mPackageManager = pm;
    }

    boolean isCallbackControllerNullForTesting() {
        return mCallbackController == null;
    }
}
