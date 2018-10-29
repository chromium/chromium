// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.download.DownloadMetrics;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.native_page.NativePageHost;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegateImpl;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.ntp.snippets.KnownCategories;
import org.chromium.chrome.browser.ntp.snippets.SnippetArticle;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.offline_items_collection.LaunchLocation;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.mojom.WindowOpenDisposition;

/**
 * Extension of {@link NativePageNavigationDelegate} with suggestions-specific methods.
 */
public class SuggestionsNavigationDelegate extends NativePageNavigationDelegateImpl {
    private static final String NEW_TAB_URL_HELP = "https://support.google.com/chrome/?p=new_tab";

    public SuggestionsNavigationDelegate(ChromeActivity activity, Profile profile,
            NativePageHost host, TabModelSelector tabModelSelector) {
        super(activity, profile, host, tabModelSelector);
    }

    /** Opens the bookmarks page in the current tab.  */
    public void navigateToBookmarks() {
        RecordUserAction.record("MobileNTPSwitchToBookmarks");
        BookmarkUtils.showBookmarkManager(mActivity);
    }

    /** Opens the Download Manager UI in the current tab. */
    public void navigateToDownloadManager() {
        RecordUserAction.record("MobileNTPSwitchToDownloadManager");
        DownloadUtils.showDownloadManager(mActivity, mHost.getActiveTab());
    }

    @Override
    public void navigateToHelpPage() {
        NewTabPageUma.recordAction(NewTabPageUma.ACTION_CLICKED_LEARN_MORE);
        // TODO(dgn): Use the standard Help UI rather than a random link to online help?
        openUrl(WindowOpenDisposition.CURRENT_TAB,
                new LoadUrlParams(NEW_TAB_URL_HELP, PageTransition.AUTO_BOOKMARK));
    }

    /**
     * Opens the suggestions page without recording metrics.
     *
     * @param windowOpenDisposition How to open (new window, current tab, etc).
     * @param url The url to navigate to.
     */
    public void navigateToSuggestionUrl(int windowOpenDisposition, String url) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url, PageTransition.AUTO_BOOKMARK);
        openUrl(windowOpenDisposition, loadUrlParams);
    }

    /**
     * Opens a content suggestion and records related metrics.
     *
     * @param windowOpenDisposition How to open (new window, current tab, etc).
     * @param article The content suggestion to open.
     */
    public void openSnippet(final int windowOpenDisposition, final SnippetArticle article) {
        if (!article.isContextual()) {
            NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_SNIPPET);
        }

        if (article.isAssetDownload()) {
            assert windowOpenDisposition == WindowOpenDisposition.CURRENT_TAB
                    || windowOpenDisposition == WindowOpenDisposition.NEW_WINDOW
                    || windowOpenDisposition == WindowOpenDisposition.NEW_BACKGROUND_TAB;
            DownloadUtils.openFile(article.getAssetDownloadFile(),
                    article.getAssetDownloadMimeType(), article.getAssetDownloadGuid(), false, null,
                    null, DownloadMetrics.DownloadOpenSource.NEW_TAP_PAGE);
            return;
        }

        // We explicitly open an offline page only for offline page downloads or for prefetched
        // offline pages when Data Reduction Proxy is enabled. For all other
        // sections the URL is opened and it is up to Offline Pages whether to open its offline
        // page (e.g. when offline).
        if ((article.isDownload() && !article.isAssetDownload())
                || (DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()
                           && article.isPrefetched())) {
            assert article.getOfflinePageOfflineId() != null;
            assert windowOpenDisposition == WindowOpenDisposition.CURRENT_TAB
                    || windowOpenDisposition == WindowOpenDisposition.NEW_WINDOW
                    || windowOpenDisposition == WindowOpenDisposition.NEW_BACKGROUND_TAB;
            OfflinePageUtils.getLoadUrlParamsForOpeningOfflineVersion(article.mUrl,
                    article.getOfflinePageOfflineId(), LaunchLocation.SUGGESTION,
                    (loadUrlParams) -> {
                        if (loadUrlParams == null) return;
                        // Extra headers are not read in loadUrl, but verbatim headers are.
                        loadUrlParams.setVerbatimHeaders(loadUrlParams.getExtraHeadersString());
                        openDownloadSuggestion(windowOpenDisposition, article, loadUrlParams);
                    });

            return;
        }

        LoadUrlParams loadUrlParams = new LoadUrlParams(article.mUrl, PageTransition.AUTO_BOOKMARK);

        // For article suggestions, we set the referrer. This is exploited
        // to filter out these history entries for NTP tiles.
        // TODO(mastiz): Extend this with support for other categories.
        if (article.mCategory == KnownCategories.ARTICLES) {
            loadUrlParams.setReferrer(new Referrer(
                    SuggestionsConfig.getReferrerUrl(ChromeFeatureList.NTP_ARTICLE_SUGGESTIONS),
                    ReferrerPolicy.ALWAYS));
        }

        // Set appropriate referrer for contextual suggestions to distinguish them from navigation
        // from a page.
        if (article.mCategory == KnownCategories.CONTEXTUAL) {
            loadUrlParams.setReferrer(
                    new Referrer(SuggestionsConfig.getReferrerUrl(
                                         ChromeFeatureList.CONTEXTUAL_SUGGESTIONS_BUTTON),
                            ReferrerPolicy.ALWAYS));
        }

        Tab loadingTab = openUrl(windowOpenDisposition, loadUrlParams);
        if (loadingTab != null && !article.isContextual()) {
            SuggestionsMetrics.recordVisit(loadingTab, article);
        }
    }

    private void openDownloadSuggestion(
            int windowOpenDisposition, SnippetArticle article, LoadUrlParams loadUrlParams) {
        Tab loadingTab = openUrl(windowOpenDisposition, loadUrlParams);
        if (loadingTab != null) SuggestionsMetrics.recordVisit(loadingTab, article);
    }
}
