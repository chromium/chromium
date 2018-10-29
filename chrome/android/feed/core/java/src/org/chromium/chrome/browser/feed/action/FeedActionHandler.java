// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.action;

import android.support.annotation.NonNull;

import com.google.android.libraries.feed.api.knowncontent.ContentMetadata;
import com.google.android.libraries.feed.host.action.ActionApi;

import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.feed.FeedLoggingBridge;
import org.chromium.chrome.browser.feed.FeedOfflineIndicator;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.suggestions.NavigationRecorder;
import org.chromium.chrome.browser.suggestions.SuggestionsConfig;
import org.chromium.chrome.browser.suggestions.SuggestionsNavigationDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;

/**
 * Handles the actions user can trigger on the feed.
 */
public class FeedActionHandler implements ActionApi {
    private final SuggestionsNavigationDelegate mDelegate;
    private final Runnable mSuggestionConsumedObserver;
    private final FeedOfflineIndicator mOfflineIndicator;
    private final OfflinePageBridge mOfflinePageBridge;
    private final FeedLoggingBridge mLoggingBridge;

    /**
     * @param delegate The {@link SuggestionsNavigationDelegate} that this handler calls when
     * handling some of the actions.
     * @param suggestionConsumedObserver An observer that is interested in any time a suggestion is
     * consumed by the user.
     * @param offlineIndicator Tracks offline pages and can supply this handler with offline ids.
     * @param offlinePageBridge Capable of updating {@link LoadUrlParams} to include offline ids.
     * @param loggingBridge Reports pages visiting time.
     */
    public FeedActionHandler(@NonNull SuggestionsNavigationDelegate delegate,
            @NonNull Runnable suggestionConsumedObserver,
            @NonNull FeedOfflineIndicator offlineIndicator,
            @NonNull OfflinePageBridge offlinePageBridge,
            @NonNull FeedLoggingBridge loggingBridge) {
        mDelegate = delegate;
        mSuggestionConsumedObserver = suggestionConsumedObserver;
        mOfflineIndicator = offlineIndicator;
        mOfflinePageBridge = offlinePageBridge;
        mLoggingBridge = loggingBridge;
    }

    @Override
    public void openUrl(String url) {
        openOfflineIfPossible(WindowOpenDisposition.CURRENT_TAB, url);
    }

    @Override
    public boolean canOpenUrl() {
        return true;
    }

    @Override
    public void openUrlInIncognitoMode(String url) {
        mDelegate.openUrl(WindowOpenDisposition.OFF_THE_RECORD, createLoadUrlParams(url));
        mSuggestionConsumedObserver.run();
    }

    @Override
    public boolean canOpenUrlInIncognitoMode() {
        return mDelegate.isOpenInIncognitoEnabled();
    }

    @Override
    public void openUrlInNewTab(String url) {
        openOfflineIfPossible(WindowOpenDisposition.NEW_BACKGROUND_TAB, url);
    }

    @Override
    public boolean canOpenUrlInNewTab() {
        return true;
    }

    @Override
    public void openUrlInNewWindow(String url) {
        openOfflineIfPossible(WindowOpenDisposition.NEW_WINDOW, url);
    }

    @Override
    public boolean canOpenUrlInNewWindow() {
        return mDelegate.isOpenInNewWindowEnabled();
    }

    @Override
    public void downloadUrl(ContentMetadata contentMetadata) {
        mDelegate.openUrl(
                WindowOpenDisposition.SAVE_TO_DISK, createLoadUrlParams(contentMetadata.getUrl()));
        mSuggestionConsumedObserver.run();
    }

    @Override
    public boolean canDownloadUrl() {
        return true;
    }

    @Override
    public void learnMore() {
        mDelegate.navigateToHelpPage();
    }

    @Override
    public boolean canLearnMore() {
        return true;
    }

    private LoadUrlParams createLoadUrlParams(String url) {
        LoadUrlParams params = new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK);
        params.setReferrer(
                new Referrer(SuggestionsConfig.getReferrerUrl(
                                     ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS),
                        ReferrerPolicy.ALWAYS));
        return params;
    }

    /**
     * Opens the given url in offline mode if possible by setting load params so the offline
     * interceptor will handle the request. If there is no offline id, fall back to just opening the
     * |url| through the |mDelegate|.
     *
     * @param disposition How to open the article. Should not be OFF_THE_RECORD, as offline pages
     * does not support opening articles while incognito.
     * @param url The url of the article. Should match what was previously requested by Feed to
     * OfflineIndicatorApi implementation exactly.
     */
    private void openOfflineIfPossible(int disposition, String url) {
        Long maybeOfflineId = mOfflineIndicator.getOfflineIdIfPageIsOfflined(url);
        if (maybeOfflineId == null) {
            Tab loadingTab = mDelegate.openUrl(disposition, createLoadUrlParams(url));
            if (loadingTab != null) {
                // Records how long the user spending on the suggested page.
                NavigationRecorder.record(loadingTab, visitData -> {
                    mLoggingBridge.onContentTargetVisited(visitData.duration);
                });
            }
        } else {
            mOfflinePageBridge.getLoadUrlParamsByOfflineId(
                    maybeOfflineId, LaunchLocation.SUGGESTION, (loadUrlParams) -> {
                        loadUrlParams.setVerbatimHeaders(loadUrlParams.getExtraHeadersString());
                        Tab loadingTab = mDelegate.openUrl(disposition, loadUrlParams);
                        if (loadingTab != null) {
                            // Records how long the user spending on the offline page.
                            NavigationRecorder.record(loadingTab, visitData -> {
                                mLoggingBridge.onOfflinePageVisited(visitData.duration);
                            });
                        }
                    });
        }
        mSuggestionConsumedObserver.run();
    }
}
