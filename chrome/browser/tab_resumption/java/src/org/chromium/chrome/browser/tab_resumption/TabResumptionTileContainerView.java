// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.DISPLAY_TEXT_MAX_LINES_DEFAULT;
import static org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.DISPLAY_TEXT_MAX_LINES_WITH_REASON;

import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Size;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.CallbackController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ClickInfo;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ModuleShowConfig;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;

import java.util.ArrayList;
import java.util.List;

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
                        .getDimensionPixelSize(R.dimen.single_tab_module_tab_thumbnail_size_big);
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
            boolean useSalientImage,
            TabModelSelector tabModelSelector,
            Tab trackingTab,
            Callback<Tab> tabObserverCallback,
            @Nullable Callback<Integer> onModuleShowConfigFinalizedCallback) {
        reset();

        @ModuleShowConfig
        Integer moduleShowConfig = TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle);

        String allTilesTexts = "";
        int entryCount = bundle.entries.size();
        boolean isSingle = entryCount == 1;

        List<Callback<TabModelSelector>> pendingCallbacks = new ArrayList<>();
        // A flag to indicate if any tile isn't finalized and we should update ModuleShowConfig.
        boolean shouldUpdateModuleShowConfig = false;
        for (SuggestionEntry entry : bundle.entries) {
            assert moduleShowConfig != null;
            @ClickInfo
            int clickInfo = TabResumptionModuleMetricsUtils.computeClickInfo(entry, entryCount);

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

            if (entry.getNeedMatchLocalTab()) {
                shouldUpdateModuleShowConfig = true;
                if (entry.getLocalTabId() == Tab.INVALID_TAB_ID) {
                    if (trackingTab != null && entry.url.equals(trackingTab.getUrl())) {
                        // If the shown Tab matches the tracking Tab, updates the entry with
                        // assigned Tab Id.
                        entry.setLocalTabId(trackingTab.getId());
                        entry.resetNeedMatchLocalTab();
                        // Registers to listen to the tab's closing event, so the tab resumption
                        // module will update if the current shown Tab is closed.
                        tabObserverCallback.onResult(trackingTab);
                        // Updates the clickInfo before it is used to setup click listener.
                        clickInfo =
                                isSingle
                                        ? ClickInfo.LOCAL_SINGLE_FIRST
                                        : ClickInfo.LOCAL_DOUBLE_ANY;
                    }
                }
            }
            if (entry.isLocalTab() && isSingle) {
                allTilesTexts +=
                        loadLocalTabSingle(
                                this,
                                entry,
                                urlImageProvider,
                                suggestionClickCallbackWithLogging,
                                ClickInfo.LOCAL_SINGLE_FIRST,
                                recencyMs);
            } else {
                int layoutId =
                        isSingle
                                ? R.layout.tab_resumption_module_single_tile_layout
                                : R.layout.tab_resumption_module_multi_tile_layout;
                TabResumptionTileView tileView =
                        (TabResumptionTileView)
                                LayoutInflater.from(getContext()).inflate(layoutId, this, false);
                allTilesTexts += loadTileTexts(entry, isSingle, tileView, recencyMs);
                loadTileUrlImage(
                        entry,
                        urlImageProvider,
                        tileView,
                        isSingle,
                        entry.type != SuggestionEntryType.HISTORY ? useSalientImage : false);
                bindSuggestionClickCallback(
                        tileView, suggestionClickCallbackWithLogging, entry, clickInfo);
                addView(tileView);

                if (entry.getNeedMatchLocalTab() && entry.getLocalTabId() == Tab.INVALID_TAB_ID) {
                    // For any history or foreign session suggestion which doesn't match the
                    // tracking Tab but still need to check, creates a callback to update the tile
                    // if a match of a local Tab is found after the tab state is initialized.
                    Callback<TabModelSelector> callback =
                            mCallbackController.makeCancelable(
                                    (tms) -> {
                                        updateTile(
                                                entry,
                                                tileView,
                                                suggestionClickCallback,
                                                tms,
                                                tabObserverCallback,
                                                entryCount);
                                    });
                    pendingCallbacks.add(callback);
                }
            }
        }

        // Setup callbacks after all suggestion(s) are iterated.
        if (!pendingCallbacks.isEmpty() || shouldUpdateModuleShowConfig) {
            handleTileMatchAndUpdate(
                    pendingCallbacks,
                    onModuleShowConfigFinalizedCallback,
                    tabModelSelector,
                    bundle,
                    shouldUpdateModuleShowConfig);
        }
        return allTilesTexts;
    }

    private void handleTileMatchAndUpdate(
            List<Callback<TabModelSelector>> pendingCallbacks,
            Callback<Integer> onModuleShowConfigFinalizedCallback,
            TabModelSelector tabModelSelector,
            SuggestionBundle bundle,
            boolean updateModuleShowConfig) {
        if (!pendingCallbacks.isEmpty()) {
            Callback<TabModelSelector> onTabStateInitializedCallback =
                    mCallbackController.makeCancelable(
                            (newTabModelSelector) -> {
                                for (var callback : pendingCallbacks) {
                                    callback.onResult(newTabModelSelector);
                                }
                                pendingCallbacks.clear();
                                onModuleShowConfigFinalizedCallback.onResult(
                                        TabResumptionModuleMetricsUtils.computeModuleShowConfig(
                                                bundle));
                            });
            TabModelUtils.runOnTabStateInitialized(tabModelSelector, onTabStateInitializedCallback);
        } else if (updateModuleShowConfig) {
            onModuleShowConfigFinalizedCallback.onResult(
                    TabResumptionModuleMetricsUtils.computeModuleShowConfig(bundle));
        }
    }

    /**
     * Called to update a TabResumptionTileView to track a local Tab. It updates the click listener
     * for the view, and register to observe the Tab's closure state.
     */
    private void updateTile(
            @NonNull SuggestionEntry entry,
            @NonNull TabResumptionTileView tileView,
            @NonNull SuggestionClickCallback suggestionClickCallback,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull Callback<Tab> tabObserverCallback,
            int size) {
        TabModel tabModel = tabModelSelector.getModel(false);
        int index = TabModelUtils.getTabIndexByUrl(tabModel, entry.url.getSpec());
        entry.resetNeedMatchLocalTab();

        if (index != TabModel.INVALID_TAB_INDEX) {
            Tab tab = tabModel.getTabAt(index);
            if (tab.getId() != Tab.INVALID_TAB_ID) {
                entry.setLocalTabId(tab.getId());
                // Registers to listen to the tab's closing event.
                tabObserverCallback.onResult(tab);
                // Updates the click listener of the tile to allow switching to an existing Tab,
                // rather than navigates within the same NTP.
                bindSuggestionClickCallback(
                        tileView,
                        suggestionClickCallback,
                        entry,
                        TabResumptionModuleMetricsUtils.computeClickInfo(entry, size));
                if (entry.type != SuggestionEntryType.LOCAL_TAB) {
                    // Removes the "device name" from the tile.
                    tileView.updatePostInfoView(TabResumptionModuleUtils.getDomainUrl(entry.url));
                }
            }
        }
    }

    /** Renders and returns the texts of a {@link TabResumptionTileView}. */
    private String loadTileTexts(
            SuggestionEntry entry,
            boolean isSingle,
            TabResumptionTileView tileView,
            long recencyMs) {
        Resources res = getContext().getResources();
        String domainUrl = TabResumptionModuleUtils.getDomainUrl(entry.url);
        boolean isHistory = entry.type == SuggestionEntryType.HISTORY;
        String postInfoText =
                entry.isLocalTab() || (isHistory && TextUtils.isEmpty(entry.sourceName))
                        ? domainUrl
                        : res.getString(
                                R.string.tab_resumption_module_domain_url_and_device_name,
                                domainUrl,
                                entry.sourceName);
        if (isSingle) {
            // Single Local Tab suggestion is handled by #loadLocalTabSingle().
            assert !entry.isLocalTab();

            String appChipText =
                    isHistory
                            ? tileView.maybeShowAppChip(mPackageManager, entry.type, entry.appId)
                            : null;
            String preInfoText =
                    appChipText == null
                            ? getReasonToShowTab(entry.reasonToShowTab, recencyMs)
                            : null;
            return tileView.setSuggestionTextsSingle(
                    preInfoText, appChipText, entry.title, postInfoText);
        }

        return tileView.setSuggestionTextsMulti(entry.title, postInfoText);
    }

    private String getReasonToShowTab(String reasonToShowTab, long recencyMs) {
        if (TabResumptionModuleUtils.TAB_RESUMPTION_SHOW_DEFAULT_REASON.getValue()
                && TextUtils.isEmpty(reasonToShowTab)) {
            String recencyString =
                    TabResumptionModuleUtils.getRecencyString(getResources(), recencyMs);
            return getContext()
                    .getString(R.string.tab_resumption_module_default_reason, recencyString);
        }
        return reasonToShowTab;
    }

    /** Loads texts and images for the single Local Tab suggestion. */
    private String loadLocalTabSingle(
            ViewGroup parentView,
            SuggestionEntry localTabEntry,
            UrlImageProvider urlImageProvider,
            SuggestionClickCallback suggestionClickCallback,
            @ClickInfo int clickInfo,
            long recencyMs) {
        assert localTabEntry.isLocalTab();
        Resources res = getContext().getResources();
        LocalTileView tileView =
                (LocalTileView)
                        LayoutInflater.from(getContext())
                                .inflate(
                                        R.layout.tab_resumption_module_local_tile_layout,
                                        this,
                                        false);
        String domainUrl = TabResumptionModuleUtils.getDomainUrl(localTabEntry.url);
        tileView.setUrl(domainUrl);
        tileView.setTitle(localTabEntry.title);

        String reason = getReasonToShowTab(localTabEntry.reasonToShowTab, recencyMs);
        boolean showReason = !TextUtils.isEmpty(reason);
        tileView.setShowReason(reason);
        tileView.setMaxLinesForTitle(
                showReason ? DISPLAY_TEXT_MAX_LINES_WITH_REASON : DISPLAY_TEXT_MAX_LINES_DEFAULT);

        urlImageProvider.fetchImageForUrl(
                localTabEntry.url,
                (Bitmap bitmap) -> {
                    Drawable urlDrawable = new BitmapDrawable(res, bitmap);
                    tileView.setFavicon(urlDrawable);
                });
        urlImageProvider.getTabThumbnail(
                localTabEntry.getLocalTabId(), mThumbnailSize, tileView::setTabThumbnail);

        bindSuggestionClickCallback(tileView, suggestionClickCallback, localTabEntry, clickInfo);

        parentView.addView(tileView);

        StringBuilder builder = new StringBuilder();
        if (showReason) {
            builder.append(reason);
            builder.append(TabResumptionTileView.SEPARATE_COMMA);
        }
        builder.append(localTabEntry.title);
        builder.append(TabResumptionTileView.SEPARATE_COMMA);
        builder.append(domainUrl);
        builder.append(TabResumptionTileView.SEPARATE_PERIOD);
        return builder.toString();
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
                                    // TODO(b/350021945): Remove check when root cause is found.
                                    if (urlImageProvider.isDestroyed()) return;

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
            // TODO(b/350021945): Remove check when root cause is found.
            if (!urlImageProvider.isDestroyed()) {
                urlImageProvider.fetchSalientImage(entry.url, isSingle, fetchSalientImageCallback);
            }
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
